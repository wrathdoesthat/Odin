#include <math.h>

// TODO(bill): Big numbers
// IMPORTANT TODO(bill): This needs to be completely fixed!!!!!!!!

enum ValueKind {
	Value_Invalid,

	Value_Bool,
	Value_String,
	Value_Integer,
	Value_Float,
	Value_Pointer, // TODO(bill): Value_Pointer

	Value_Count,
};

struct Value {
	ValueKind kind;
	union {
		b32    value_bool;
		String value_string;
		i64    value_integer;
		f64    value_float;
		void * value_pointer;
	};
};

Value make_value_bool(b32 b) {
	Value result = {Value_Bool};
	result.value_bool = (b != 0);
	return result;
}

Value make_value_string(String string) {
	// TODO(bill): Allow for numbers with underscores in them
	Value result = {Value_String};
	result.value_string = string;
	return result;
}

Value make_value_integer(String string) {
	// TODO(bill): Allow for numbers with underscores in them
	Value result = {Value_Integer};
	i32 base = 10;
	if (string.text[0] == '0') {
		switch (string.text[1]) {
		case 'b': base = 2;  break;
		case 'o': base = 8;  break;
		case 'd': base = 10; break;
		case 'x': base = 16; break;
		}
	}

	result.value_integer = gb_str_to_i64(cast(char *)string.text, NULL, base);

	return result;
}

Value make_value_integer(i64 i) {
	Value result = {Value_Integer};
	result.value_integer = i;
	return result;
}

Value make_value_float(String string) {
	// TODO(bill): Allow for numbers with underscores in them
	Value result = {Value_Float};
	result.value_float = gb_str_to_f64(cast(char *)string.text, NULL);
	return result;
}

Value make_value_float(f64 f) {
	Value result = {Value_Float};
	result.value_float = f;
	return result;
}

Value make_value_pointer(void *ptr) {
	Value result = {Value_Pointer};
	result.value_pointer = ptr;
	return result;
}

Value make_value_from_basic_literal(Token token) {
	switch (token.kind) {
	case Token_String:  return make_value_string(token.string);
	case Token_Integer: return make_value_integer(token.string);
	case Token_Float:   return make_value_float(token.string);
	case Token_Rune:    return make_value_integer(token.string);
	default:
		GB_PANIC("Invalid token for basic literal");
		break;
	}

	Value result = {Value_Invalid};
	return result;
}

Value value_to_integer(Value v) {
	switch (v.kind) {
	case Value_Integer:
		return v;
	case Value_Float:
		return make_value_integer(cast(i64)v.value_float);
	}
	Value r = {Value_Invalid};
	return r;
}

Value value_to_float(Value v) {
	switch (v.kind) {
	case Value_Integer:
		return make_value_float(cast(i64)v.value_integer);
	case Value_Float:
		return v;
	}
	Value r = {Value_Invalid};
	return r;
}


Value unary_operator_value(Token op, Value v, i32 precision) {
	switch (op.kind) {
	case Token_Add:	{
		switch (v.kind) {
		case Value_Invalid:
		case Value_Integer:
		case Value_Float:
			return v;
		}
	} break;

	case Token_Sub:	{
		switch (v.kind) {
		case Value_Invalid:
			return v;
		case Value_Integer: {
			Value i = v;
			i.value_integer = -i.value_integer;
			return i;
		}
		case Value_Float: {
			Value i = v;
			i.value_float = -i.value_float;
			return i;
		}
		}
	} break;

	case Token_Xor: {
		i64 i = 0;
		switch (v.kind) {
		case Value_Invalid:
			return v;
		case Value_Integer:
			i = ~i;
			break;
		default:
			goto failure;
		}

		// NOTE(bill): unsigned integers will be negative and will need to be
		// limited to the types precision
		if (precision > 0)
			i &= ~((-1)<<precision);

		return make_value_integer(i);
	} break;

	case Token_Not: {
		switch (v.kind) {
		case Value_Invalid: return v;
		case Value_Bool:
			return make_value_bool(!v.value_bool);
		}
	} break;
	}

failure:
	GB_PANIC("Invalid unary operation, %s", token_kind_to_string(op.kind));

	Value error_value = {};
	return error_value;
}

// NOTE(bill): Make sure things are evaluated in correct order
i32 value_order(Value v) {
	switch (v.kind) {
	case Value_Invalid:
		return 0;
	case Value_Bool:
	case Value_String:
		return 1;
	case Value_Integer:
		return 2;
	case Value_Float:
		return 3;
	case Value_Pointer:
		return 4;

	default:
		GB_PANIC("How'd you get here? Invalid Value.kind");
		return -1;
	}
}

void match_values(Value *x, Value *y) {
	if (value_order(*y) < value_order(*x)) {
		match_values(y, x);
		return;
	}

	switch (x->kind) {
	case Value_Invalid:
		*y = *x;
		return;

	case Value_Bool:
	case Value_String:
		return;

	case Value_Integer: {
		switch (y->kind) {
		case Value_Integer:
			return;
		case Value_Float:
			// TODO(bill): Is this good enough?
			*x = make_value_float(cast(f64)x->value_integer);
			return;
		}
	} break;

	case Value_Float: {
		if (y->kind == Value_Float)
			return;
	} break;
	}

	GB_PANIC("How'd you get here? Invalid Value.kind");
}

Value binary_operator_value(Token op, Value x, Value y) {
	match_values(&x, &y);

	switch (x.kind) {
	case Value_Invalid:
		return x;

	case Value_Bool:
		switch (op.kind) {
		case Token_CmpAnd: return make_value_bool(x.value_bool && y.value_bool);
		case Token_CmpOr:  return make_value_bool(x.value_bool || y.value_bool);
		default: goto error;
		}
		break;

	case Value_Integer: {
		i64 a = x.value_integer;
		i64 b = y.value_integer;
		i64 c = 0;
		switch (op.kind) {
		case Token_Add:    c = a + b;  break;
		case Token_Sub:    c = a - b;  break;
		case Token_Mul:    c = a * b;  break;
		case Token_Quo:    return make_value_float(fmod(cast(f64)a, cast(f64)b));
		case Token_QuoEq:  c = a / b;  break; // NOTE(bill): Integer division
		case Token_Mod:    c = a % b;  break;
		case Token_And:    c = a & b;  break;
		case Token_Or:     c = a | b;  break;
		case Token_Xor:    c = a ^ b;  break;
		case Token_AndNot: c = a&(~b); break;
		default: goto error;
		}
		return make_value_integer(c);
	} break;

	case Value_Float: {
		f64 a = x.value_float;
		f64 b = y.value_float;
		switch (op.kind) {
		case Token_Add: return make_value_float(a + b);
		case Token_Sub: return make_value_float(a - b);
		case Token_Mul: return make_value_float(a * b);
		case Token_Quo: return make_value_float(a / b);
		default: goto error;
		}
	} break;
	}

error:
	Value error_value = {};
	GB_PANIC("Invalid binary operation: %s", token_kind_to_string(op.kind));
	return error_value;
}

gb_inline Value value_add(Value x, Value y) { Token op = {Token_Add}; return binary_operator_value(op, x, y); }
gb_inline Value value_sub(Value x, Value y) { Token op = {Token_Sub}; return binary_operator_value(op, x, y); }
gb_inline Value value_mul(Value x, Value y) { Token op = {Token_Mul}; return binary_operator_value(op, x, y); }
gb_inline Value value_quo(Value x, Value y) { Token op = {Token_Quo}; return binary_operator_value(op, x, y); }


i32 cmp_f64(f64 a, f64 b) {
	return (a > b) - (a < b);
}

b32 compare_values(Token op, Value x, Value y) {
	match_values(&x, &y);

	switch (x.kind) {
	case Value_Invalid:
		return false;

	case Value_Bool:
		switch (op.kind) {
		case Token_CmpEq: return x.value_bool == y.value_bool;
		case Token_NotEq: return x.value_bool != y.value_bool;
		}
		break;

	case Value_Integer: {
		i64 a = x.value_integer;
		i64 b = y.value_integer;
		switch (op.kind) {
		case Token_CmpEq: return a == b;
		case Token_NotEq: return a != b;
		case Token_Lt:    return a <  b;
		case Token_LtEq:  return a <= b;
		case Token_Gt:    return a >  b;
		case Token_GtEq:  return a >= b;
		}
	} break;

	case Value_Float: {
		f64 a = x.value_float;
		f64 b = y.value_float;
		switch (op.kind) {
		case Token_CmpEq: return cmp_f64(a, b) == 0;
		case Token_NotEq: return cmp_f64(a, b) != 0;
		case Token_Lt:    return cmp_f64(a, b) <  0;
		case Token_LtEq:  return cmp_f64(a, b) <= 0;
		case Token_Gt:    return cmp_f64(a, b) >  0;
		case Token_GtEq:  return cmp_f64(a, b) >= 0;
		}
	} break;

	case Value_String: {
		String a = x.value_string;
		String b = y.value_string;
		isize len = gb_min(a.len, b.len);
		// TODO(bill): gb_memcompare is used because the strings are UTF-8
		switch (op.kind) {
		case Token_CmpEq: return gb_memcompare(a.text, b.text, len) == 0;
		case Token_NotEq: return gb_memcompare(a.text, b.text, len) != 0;
		case Token_Lt:    return gb_memcompare(a.text, b.text, len) <  0;
		case Token_LtEq:  return gb_memcompare(a.text, b.text, len) <= 0;
		case Token_Gt:    return gb_memcompare(a.text, b.text, len) >  0;
		case Token_GtEq:  return gb_memcompare(a.text, b.text, len) >= 0;
		}
	} break;
	}

	GB_PANIC("Invalid comparison");
	return false;
}
