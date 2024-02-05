#include "NodeTools.hpp"

#include "openvic-simulation/types/Colour.hpp"

using namespace OpenVic;
using namespace OpenVic::NodeTools;

template<typename T>
static NodeCallback auto _expect_type(Callback<T const&> auto callback) {
	return [callback](ast::NodeCPtr node) -> bool {
		if (node != nullptr) {
			T const* cast_node = node->cast_to<T>();
			if (cast_node != nullptr) {
				return callback(*cast_node);
			}
			Logger::error("Invalid node type ", node->get_type(), " when expecting ", T::get_type_static());
		} else {
			Logger::error("Null node when expecting ", T::get_type_static());
		}
		return false;
	};
}

template<std::derived_from<ast::AbstractStringNode> T>
static Callback<T const&> auto _abstract_string_node_callback(Callback<std::string_view> auto callback, bool allow_empty) {
	return [callback, allow_empty](T const& node) -> bool {
		if (allow_empty) {
			return callback(node._name);
		} else {
			if (!node._name.empty()) {
				return callback(node._name);
			} else {
				Logger::error("Invalid string value - empty!");
				return false;
			}
		}
	};
}

node_callback_t NodeTools::expect_identifier(callback_t<std::string_view> callback) {
	return _expect_type<ast::IdentifierNode>(_abstract_string_node_callback<ast::IdentifierNode>(callback, false));
}

node_callback_t NodeTools::expect_string(callback_t<std::string_view> callback, bool allow_empty) {
	return _expect_type<ast::StringNode>(_abstract_string_node_callback<ast::StringNode>(callback, allow_empty));
}

node_callback_t NodeTools::expect_identifier_or_string(callback_t<std::string_view> callback, bool allow_empty) {
	return [callback, allow_empty](ast::NodeCPtr node) -> bool {
		if (node != nullptr) {
			ast::AbstractStringNode const* cast_node = node->cast_to<ast::IdentifierNode>();
			if (cast_node == nullptr) {
				cast_node = node->cast_to<ast::StringNode>();
			}
			if (cast_node != nullptr) {
				return _abstract_string_node_callback<ast::AbstractStringNode>(callback, allow_empty)(*cast_node);
			}
			Logger::error(
				"Invalid node type ", node->get_type(), " when expecting ", ast::IdentifierNode::get_type_static(), " or ",
				ast::StringNode::get_type_static()
			);
		} else {
			Logger::error(
				"Null node when expecting ", ast::IdentifierNode::get_type_static(), " or ", ast::StringNode::get_type_static()
			);
		}
		return false;
	};
}

node_callback_t NodeTools::expect_bool(callback_t<bool> callback) {
	static const case_insensitive_string_map_t<bool> bool_map { { "yes", true }, { "no", false } };
	return expect_identifier(expect_mapped_string(bool_map, callback));
}

node_callback_t NodeTools::expect_int_bool(callback_t<bool> callback) {
	return expect_uint64([callback](uint64_t num) -> bool {
		if (num > 1) {
			Logger::warning("Found int bool with value >1: ", num);
		}
		return callback(num != 0);
	});
}

node_callback_t NodeTools::expect_int64(callback_t<int64_t> callback, int base) {
	return expect_identifier([callback, base](std::string_view identifier) -> bool {
		bool successful = false;
		const int64_t val = StringUtils::string_to_int64(identifier, &successful, base);
		if (successful) {
			return callback(val);
		}
		Logger::error("Invalid int identifier text: ", identifier);
		return false;
	});
}

node_callback_t NodeTools::expect_uint64(callback_t<uint64_t> callback, int base) {
	return expect_identifier([callback, base](std::string_view identifier) -> bool {
		bool successful = false;
		const uint64_t val = StringUtils::string_to_uint64(identifier, &successful, base);
		if (successful) {
			return callback(val);
		}
		Logger::error("Invalid uint identifier text: ", identifier);
		return false;
	});
}

callback_t<std::string_view> NodeTools::expect_fixed_point_str(callback_t<fixed_point_t> callback) {
	return [callback](std::string_view identifier) -> bool {
		bool successful = false;
		const fixed_point_t val = fixed_point_t::parse(identifier.data(), identifier.length(), &successful);
		if (successful) {
			return callback(val);
		}
		Logger::error("Invalid fixed point identifier text: ", identifier);
		return false;
	};
}

node_callback_t NodeTools::expect_fixed_point(callback_t<fixed_point_t> callback) {
	return expect_identifier(expect_fixed_point_str(callback));
}

node_callback_t NodeTools::expect_colour(callback_t<colour_t> callback) {
	return [callback](ast::NodeCPtr node) -> bool {
		colour_t col = colour_t::null();
		int32_t components = 0;
		bool ret = expect_list_of_length(3, expect_fixed_point(
			[&col, &components](fixed_point_t val) -> bool {
				if (val < 0 || val > 255) {
					Logger::error("Invalid colour component #", components++, ": ", val);
					return false;
				} else {
					if (val <= 1) {
						val *= 255;
					} else if (!val.is_integer()) {
						Logger::warning("Fractional part of colour component #", components, " will be truncated: ", val);
					}
					col[components++] = val.to_int64_t();
					return true;
				}
			}
		))(node);
		ret &= callback(col);
		return ret;
	};
}

node_callback_t NodeTools::expect_colour_hex(callback_t<colour_argb_t> callback) {
	return expect_uint<colour_argb_t::integer_type>([callback](colour_argb_t::integer_type val) -> bool {
		return callback(colour_argb_t::from_integer(val));
	}, 16);
}

callback_t<std::string_view> NodeTools::expect_date_str(callback_t<Date> callback) {
	return [callback](std::string_view identifier) -> bool {
		bool successful = false;
		const Date date = Date::from_string(identifier, &successful);
		if (successful) {
			return callback(date);
		}
		Logger::error("Invalid date identifier text: ", identifier);
		return false;
	};
}

node_callback_t NodeTools::expect_date(callback_t<Date> callback) {
	return expect_identifier(expect_date_str(callback));
}

node_callback_t NodeTools::expect_date_string(callback_t<Date> callback) {
	return expect_string(expect_date_str(callback));
}

node_callback_t NodeTools::expect_date_identifier_or_string(callback_t<Date> callback) {
	return expect_identifier_or_string(expect_date_str(callback));
}

node_callback_t NodeTools::expect_years(callback_t<Timespan> callback) {
	return expect_uint<Timespan::day_t>([callback](Timespan::day_t val) -> bool {
		return callback(Timespan::from_years(val));
	});
}

node_callback_t NodeTools::expect_months(callback_t<Timespan> callback) {
	return expect_uint<Timespan::day_t>([callback](Timespan::day_t val) -> bool {
		return callback(Timespan::from_months(val));
	});
}

node_callback_t NodeTools::expect_days(callback_t<Timespan> callback) {
	return expect_uint<Timespan::day_t>([callback](Timespan::day_t val) -> bool {
		return callback(Timespan::from_days(val));
	});
}

template<typename T, node_callback_t (*expect_func)(callback_t<T>)>
NodeCallback auto _expect_vec2(Callback<vec2_t<T>> auto callback) {
	return [callback](ast::NodeCPtr node) -> bool {
		vec2_t<T> vec;
		bool ret = expect_dictionary_keys(
			"x", ONE_EXACTLY, expect_func(assign_variable_callback(vec.x)),
			"y", ONE_EXACTLY, expect_func(assign_variable_callback(vec.y))
		)(node);
		ret &= callback(vec);
		return ret;
	};
}

static node_callback_t _expect_int(callback_t<ivec2_t::type> callback) {
	return expect_int(callback);
}

node_callback_t NodeTools::expect_ivec2(callback_t<ivec2_t> callback) {
	return _expect_vec2<ivec2_t::type, _expect_int>(callback);
}

node_callback_t NodeTools::expect_fvec2(callback_t<fvec2_t> callback) {
	return _expect_vec2<fixed_point_t, expect_fixed_point>(callback);
}

node_callback_t NodeTools::expect_assign(key_value_callback_t callback) {
	return _expect_type<ast::AssignNode>([callback](ast::AssignNode const& assign_node) -> bool {
		const bool ret = callback(assign_node._name, assign_node._initializer.get());
		if (!ret) {
			Logger::error("Callback failed for assign node with key: ", assign_node._name);
		}
		return ret;
	});
}

node_callback_t NodeTools::expect_list_and_length(length_callback_t length_callback, node_callback_t callback) {
	return _expect_type<ast::AbstractListNode>([length_callback, callback](ast::AbstractListNode const& list_node) -> bool {
		std::vector<ast::NodeUPtr> const& list = list_node._statements;
		bool ret = true;
		size_t size = length_callback(list.size());
		if (size > list.size()) {
			Logger::error("Trying to read more values than the list contains: ", size, " > ", list.size());
			size = list.size();
			ret = false;
		}
		std::for_each(list.begin(), list.begin() + size, [callback, &ret](ast::NodeUPtr const& sub_node) -> void {
			ret &= callback(sub_node.get());
		});
		return ret;
	});
}

node_callback_t NodeTools::expect_list_of_length(size_t length, node_callback_t callback) {
	return [length, callback](ast::NodeCPtr node) -> bool {
		bool ret = true;
		ret &= expect_list_and_length(
			[length, &ret](size_t size) -> size_t {
				if (size != length) {
					Logger::error("List length ", size, " does not match expected length ", length);
					ret = false;
					if (length < size) {
						return length;
					}
				}
				return size;
			},
			callback
		)(node);
		return ret;
	};
}

node_callback_t NodeTools::expect_list(node_callback_t callback) {
	return expect_list_and_length(default_length_callback, callback);
}

node_callback_t NodeTools::expect_length(callback_t<size_t> callback) {
	return [callback](ast::NodeCPtr node) -> bool {
		bool ret = true;
		ret &= expect_list_and_length(
			[callback, &ret](size_t size) -> size_t {
				ret &= callback(size);
				return 0;
			},
			success_callback
		)(node);
		return ret;
	};
}

node_callback_t NodeTools::expect_key(std::string_view key, node_callback_t callback, bool* key_found, bool allow_duplicates) {
	return _expect_type<ast::AbstractListNode>(
		[key, callback, key_found, allow_duplicates](ast::AbstractListNode const& list_node) -> bool {
			bool ret = true;
			size_t keys_found = 0;
			std::vector<ast::NodeUPtr> const& list = list_node._statements;
			for (ast::NodeUPtr const& sub_node : list_node._statements) {
				ast::AssignNode const* assign_node = sub_node->cast_to<ast::AssignNode>();
				if (assign_node != nullptr && assign_node->_name == key) {
					if (keys_found++ == 0) {
						ret &= callback(&*assign_node->_initializer);
						if (allow_duplicates) {
							break;
						}
					}
				}
			}
			if (keys_found == 0) {
				if (key_found != nullptr) {
					*key_found = false;
				} else {
					Logger::error("Failed to find expected key: \"", key, "\"");
				}
				ret = false;
			} else {
				if (key_found != nullptr) {
					*key_found = true;
				}
				if (!allow_duplicates && keys_found > 1) {
					Logger::error("Found ", keys_found, " instances of key: \"", key, "\" (expected 1)");
					ret = false;
				}
			}
			return ret;
		}
	);
}

node_callback_t NodeTools::expect_dictionary_and_length(length_callback_t length_callback, key_value_callback_t callback) {
	return expect_list_and_length(length_callback, expect_assign(callback));
}

node_callback_t NodeTools::expect_dictionary(key_value_callback_t callback) {
	return expect_dictionary_and_length(default_length_callback, callback);
}

node_callback_t NodeTools::name_list_callback(callback_t<name_list_t&&> callback) {
	return [callback](ast::NodeCPtr node) -> bool {
		name_list_t list;
		bool ret = expect_list_reserve_length(
			list, expect_identifier_or_string(vector_callback<std::string_view>(list))
		)(node);
		ret &= callback(std::move(list));
		return ret;
	};
}

std::ostream& OpenVic::operator<<(std::ostream& stream, name_list_t const& name_list) {
	stream << '[';
	if (!name_list.empty()) {
		stream << name_list.front();
		std::for_each(name_list.begin() + 1, name_list.end(), [&stream](std::string const& name) -> void {
			stream << ", " << name;
		});
	}
	return stream << ']';
}

callback_t<std::string_view> NodeTools::assign_variable_callback_string(std::string& var) {
	return assign_variable_callback_cast<std::string_view>(var);
}
