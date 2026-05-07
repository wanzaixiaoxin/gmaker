package handler

import (
	"encoding/json"
	"fmt"
	"strconv"
)

// ValidateJSONSchema 轻量级 JSON Schema 校验
// 支持：type、required、properties（单层）
func ValidateJSONSchema(content string, schemaJSON string) error {
	if schemaJSON == "" {
		return nil // 无 schema，跳过校验
	}

	var schema map[string]interface{}
	if err := json.Unmarshal([]byte(schemaJSON), &schema); err != nil {
		return fmt.Errorf("invalid schema json: %w", err)
	}

	var data interface{}
	if err := json.Unmarshal([]byte(content), &data); err != nil {
		return fmt.Errorf("invalid content json: %w", err)
	}

	return validateValue(data, schema, "")
}

func validateValue(data interface{}, schema map[string]interface{}, path string) error {
	// 校验 type
	if typeVal, ok := schema["type"].(string); ok && typeVal != "" {
		if err := checkType(data, typeVal, path); err != nil {
			return err
		}
	}

	// 如果是对象，校验 required 和 properties
	if obj, ok := data.(map[string]interface{}); ok {
		// required
		if reqArr, ok := schema["required"].([]interface{}); ok {
			for _, r := range reqArr {
				if key, ok := r.(string); ok {
					if _, exists := obj[key]; !exists {
						return fmt.Errorf("%s: missing required field %q", path, key)
					}
				}
			}
		}

		// properties
		if props, ok := schema["properties"].(map[string]interface{}); ok {
			for key, val := range obj {
				if propSchema, ok := props[key].(map[string]interface{}); ok {
					childPath := key
					if path != "" {
						childPath = path + "." + key
					}
					if err := validateValue(val, propSchema, childPath); err != nil {
						return err
					}
				}
			}
		}
	}

	// 如果是数组，校验 items
	if arr, ok := data.([]interface{}); ok {
		if itemsSchema, ok := schema["items"].(map[string]interface{}); ok {
			for i, item := range arr {
				childPath := fmt.Sprintf("%s[%d]", path, i)
				if err := validateValue(item, itemsSchema, childPath); err != nil {
					return err
				}
			}
		}
	}

	// enum 校验
	if enumArr, ok := schema["enum"].([]interface{}); ok {
		found := false
		for _, ev := range enumArr {
			if fmt.Sprintf("%v", ev) == fmt.Sprintf("%v", data) {
				found = true
				break
			}
		}
		if !found {
			return fmt.Errorf("%s: value %v not in enum %v", path, data, enumArr)
		}
	}

	// minimum / maximum 校验（number/integer）
	if num, ok := toFloat64(data); ok {
		if min, ok := schema["minimum"].(float64); ok && num < min {
			return fmt.Errorf("%s: value %v < minimum %v", path, num, min)
		}
		if max, ok := schema["maximum"].(float64); ok && num > max {
			return fmt.Errorf("%s: value %v > maximum %v", path, num, max)
		}
	}

	// minLength / maxLength 校验（string）
	if s, ok := data.(string); ok {
		if minLen, ok := schema["minLength"].(float64); ok && float64(len(s)) < minLen {
			return fmt.Errorf("%s: string length %d < minLength %d", path, len(s), int(minLen))
		}
		if maxLen, ok := schema["maxLength"].(float64); ok && float64(len(s)) > maxLen {
			return fmt.Errorf("%s: string length %d > maxLength %d", path, len(s), int(maxLen))
		}
	}

	return nil
}

func checkType(data interface{}, expected string, path string) error {
	var actual string
	switch data.(type) {
	case string:
		actual = "string"
	case float64:
		actual = "number"
	case bool:
		actual = "boolean"
	case []interface{}:
		actual = "array"
	case map[string]interface{}:
		actual = "object"
	case nil:
		actual = "null"
	default:
		actual = fmt.Sprintf("%T", data)
	}

	// integer 是 number 的子集
	if expected == "integer" && actual == "number" {
		if n, ok := data.(float64); ok {
			if n == float64(int64(n)) {
				return nil
			}
		}
		return fmt.Errorf("%s: expected integer, got %v", path, data)
	}

	if actual != expected {
		return fmt.Errorf("%s: expected type %q, got %q", path, expected, actual)
	}
	return nil
}

func toFloat64(v interface{}) (float64, bool) {
	switch n := v.(type) {
	case float64:
		return n, true
	case int:
		return float64(n), true
	case int64:
		return float64(n), true
	case string:
		if f, err := strconv.ParseFloat(n, 64); err == nil {
			return f, true
		}
	}
	return 0, false
}
