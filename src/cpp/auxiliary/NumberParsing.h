#ifndef NUMBER_PARSING_H
#define NUMBER_PARSING_H

#include <algorithm>
#include <cassert>
#include <tuple>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace Aux {
namespace Parsing {


namespace Impl {
	
	template<typename CharIterator>
	std::tuple<CharIterator, char> dropSpaces(CharIterator it, CharIterator end);
	
	template<typename Integer>
	double powerOf10(Integer exp);
	
	class IntegerTag{};
	
	template<typename Integer, typename CharIterator, typename ValidationPolicy>
	std::tuple<Integer, CharIterator> strTo(CharIterator it, CharIterator end,
			IntegerTag);
	
	class RealTag{};
	
	template<typename Real, typename CharIterator, typename ValidationPolicy>
	std::tuple<Real, CharIterator> strTo(CharIterator it, CharIterator end, RealTag);
	
	template<typename T>
	using ArithmeticTag = typename std::conditional<
		std::is_integral<T>::value,
			IntegerTag,
			typename std::conditional<std::is_floating_point<T>::value,
				RealTag, void>::type
		>::type;
	
	struct DefaultChecking {
		static void enforce(bool b) {
			assert(b);
			std::ignore = b; // shut up warnings in release-mode
		}
	};
} // namespace Impl


/**
 * Parses a range of characters as number.
 *
 * @param Number must be either a floating-point-type or an integer-type
 * @param CharIterator must be a valid input-iterator over a type that is
 *                     implicitly convertable to char
 * @param ValidationPolicy Must be a type that provides a static member-function 'enforce' that
 *                         recieves an argument of type bool. The default will call assert on it's
 *                         argument.
 *
 * @param it the start of the character-range
 * @param end the end of the character-range
 *
 * Requirements: The range [it, end) must contain a valid number. 
 *
 * @returns: A tuple of the parsed value and the iterator after parsing the number and dropping
 *           any surrounding whitespace.
 *
 */
template<typename Number, typename CharIterator, typename ValidationPolicy = Impl::DefaultChecking>
std::tuple<Number, CharIterator> strTo(CharIterator it, CharIterator end) {
	return Impl::strTo<Number, CharIterator, ValidationPolicy>(it, end, Impl::ArithmeticTag<Number>{});
}



namespace Impl {

template<typename Integer, typename CharIterator, typename ValidationPolicy>
std::tuple<Integer, CharIterator> strTo(CharIterator it, const CharIterator end, IntegerTag) {
	using Impl::dropSpaces;
	ValidationPolicy::enforce(it != end);
	
	char c = *it;
	std::tie(it, c) = dropSpaces(it, end);
	
	bool isNegative = false;
	if(std::is_signed<Integer>::value) { // this should be optimized away entirely
		switch (c) {
			case '-':
				isNegative = true;
				// fallthrough
			case '+':
				++it;
				if (it == end) {
					throw std::invalid_argument{
						"string contains no digits after sign"};
				}
				c = *it;
				break;
			default:
				break;
		}
	}
	
	if(!isdigit(c)) {
		throw std::invalid_argument{"string contains no digits"};
	}
	
	Integer val = 0;
	while (true) {
		ValidationPolicy::enforce(std::numeric_limits<Integer>::max() / 10 >= val);
		
		val *= 10;
		
		c -= '0';
		ValidationPolicy::enforce(std::numeric_limits<Integer>::max() - c >= val);
		
		val += c;
		
		++it;
		if(it == end) {
			break;
		}
		c = *it;
		if(!isdigit(c)) {
			break;
		}
	}
	
	if(isNegative) {
		val = -val;
	}
	
	std::tie(it, c) = dropSpaces(it, end);
	
	return std::make_tuple(val, it);
}

template<typename Real, typename CharIterator, typename ValidationPolicy>
std::tuple<Real, CharIterator> strTo(CharIterator it, const CharIterator end, RealTag) {
	
	static_assert(std::numeric_limits<Real>::max_digits10
			<= std::numeric_limits<std::uintmax_t>::digits10,
			"can't safe mantissa in biggest integer");
	
	using Impl::dropSpaces;
	using Impl::powerOf10;
	
	char c;
	bool isNegative = false;
	std::uintmax_t mantissa = 0;
	int exp = 0;
	
	auto makeReturnValue = [&]() {
		Real fp_mantissa = mantissa;
		Real value = fp_mantissa * powerOf10(exp);
		if (isNegative) {
			value = -value;
		}
		return std::make_tuple(value, it);
	};
	
	// drop whitespace:
	std::tie(it, c) = dropSpaces(it, end);
	
	ValidationPolicy::enforce(it != end);
	
	// set sign:
	switch (c) {
		case '-':
			isNegative = true;
			// fallthrough
		case '+':
			++it;
			if (it == end) {
				throw std::invalid_argument{"string contains no digits"};
			}
			c = *it;
			break;
		default:
			break;
	}
	
	// number of decimal digits that can be stored in the mantissa and the used integer
	unsigned remainingDigits = std::numeric_limits<Real>::max_digits10;
	
	//read 'big' part of the mantissa:
	while (remainingDigits > 0) {
		if (!isdigit(c)) {
			break;
		}
		--remainingDigits;
		mantissa *= 10;
		mantissa += c - '0';
		++it;
		if (it == end) {
			makeReturnValue();
		}
		c = *it;
	}
	if(remainingDigits == 0) {
		if(isdigit(c)) {
			//round correctly:
			if (c - '0' >= 5) {
				++mantissa;
			}
		}
		while (isdigit(c)) {
			++exp;
			++it;
			if (it == end) {
				break;
			}
			c = *it;
		}
	}
	// read 'small' part of the mantissa
	if (c == '.') {
		++it;
		if(it  == end) {
			return makeReturnValue();
		}
		c = *it;
		while (remainingDigits > 0) {
			if (!isdigit(c)) {
				break;
			}
			--exp;
			--remainingDigits;
			mantissa *= 10;
			mantissa += c - '0';
			++it;
			if (it == end) {
				return makeReturnValue();
			}
			c = *it;
		}
		if (isdigit(c)) {
			//round correctly:
			if (c - '0' >= 5) {
				++mantissa;
			}
		}
		// skip the remaining digits:
		it = std::find_if_not(it, end, isdigit);
		if (it == end) {
			return makeReturnValue();
		}
		c = *it;
	}
	
	// calculate the final exponent:
	if (c == 'e' || c == 'E') {
		++it;
		int tmp;
		// we need to pass IntegerTag explicitly here because we are in a
		// nested namespace. This shouldn't be required anywhere else
		std::tie(tmp, it) = strTo<int, CharIterator, ValidationPolicy>(it, end, IntegerTag{});
		exp += tmp;
	}
	
	// drop further whitespace:
	std::tie(it, c) = dropSpaces(it, end);
	
	return makeReturnValue();
}

template<typename CharIterator>
std::tuple<CharIterator, char> dropSpaces(CharIterator it, CharIterator end) {
	char c = 0;
	while(it != end && std::isspace((c = *it))) {
		++it;
	}
	return std::make_tuple(it, c);
}

template<typename Integer>
double powerOf10(Integer exp) {
	if (exp < 0) {
		return 1.0 / powerOf10(-exp);
	}
	switch(exp) {
		case 0: return 1;
		case 1: return 10;
		case 2: return 100;
		case 3: return 1000;
		case 4: return 10000;
		case 5: return 100000;
		case 6: return 1000000;
		case 7: return 10000000;
		case 8: return 100000000;
		case 9: return 1000000000;
		default:
			auto tmp = powerOf10(exp / 2);
			if (exp % 2 == 0) {
				return tmp * tmp;
			} else {
				return tmp * tmp * 10;
			}
	}
}

} // namespace Impl

}} // namespace Aux::Parsing

#endif