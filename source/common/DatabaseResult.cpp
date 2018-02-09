/*  
    EQ2Emulator:  Everquest II Server Emulator
    Copyright (C) 2007  EQ2EMulator Development Team (http://www.eq2emulator.net)

    This file is part of EQ2Emulator.

    EQ2Emulator is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    EQ2Emulator is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with EQ2Emulator.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include <string.h>
#include "Log.h"
#include "DatabaseResult.h"

//enforced by MySQL...couldn't find a #define in their headers though
#define FIELD_NAME_MAX	64

//return this instead of NULL for certain functions to prevent crashes from coding errors
static const char *empty_str = "";

DatabaseResult::DatabaseResult() {
	result = NULL;
	field_names = NULL;
	num_fields = 0;
	row = 0;
}

DatabaseResult::~DatabaseResult() {
	unsigned int i;

	if (result != NULL)
		mysql_free_result(result);

	if (field_names != NULL) {
		for (i = 0; i < num_fields; i++)
			free(field_names[i]);
		free(field_names);
	}
}

bool DatabaseResult::StoreResult(MYSQL_RES *res) {
	unsigned int i, j;
	MYSQL_FIELD *field;

	//clear any previously stored result
	if (result != NULL)
		mysql_free_result(result);

	//clear any field names from a previous result
	if (field_names != NULL) {
		for (i = 0; i < num_fields; i++)
			free(field_names[i]);
		free(field_names);
	}

	//store the new result
	result = res;
	num_fields = mysql_num_fields(res);
	
	//allocate enough space for each field's name
	if ((field_names = (char **)malloc(sizeof(char *) * num_fields)) == NULL) {
		LogWrite(DATABASE__ERROR, 0, "Database Result", "Unable to allocate %u bytes when storing database result and fetching field names\nNumber of fields=%u\n", sizeof(char *) * num_fields, num_fields);
		mysql_free_result(result);
		result = NULL;
		num_fields = 0;
		return false;
	}

	//store each field's name
	i = 0;
	while ((field = mysql_fetch_field(result)) != NULL) {
		if ((field_names[i] = (char *)calloc(field->name_length + 1, sizeof(char))) == NULL) {
			LogWrite(DATABASE__ERROR, 0, "Database Result", "Unable to allocate %u bytes when storing databse result and copying field name %s\n", field->name_length + 1, field->name);
			mysql_free_result(result);
			result = NULL;
			for (j = 0; j < i; j++)
				free(field_names[j]);
			free(field_names);
			field_names = NULL;
			num_fields = 0;
			return false;
		}

		strncpy(field_names[i], field->name, field->name_length);
		i++;
	}

	return true;
}

const char * DatabaseResult::GetFieldValue(unsigned int index) {
	if (index >= num_fields) {
		LogWrite(DATABASE__ERROR, 0, "Database Result", "Attempt to access field at index %u but there %s only %u field%s", index, num_fields == 1 ? "is" : "are", num_fields, num_fields == 1 ? "" : "s");
		return NULL;
	}

	return row[index];
}

const char * DatabaseResult::GetFieldValueStr(const char *field_name) {
	unsigned int i;

	for (i = 0; i < num_fields; i++) {
		if (strncmp(field_name, field_names[i], FIELD_NAME_MAX) == 0)
			return row[i];
	}

	LogWrite(DATABASE__ERROR, 0, "Database Result", "Unknown field name '%s'", field_name);
	return NULL;
}

bool DatabaseResult::Next() {
	return (result != NULL && (row = mysql_fetch_row(result)) != NULL);
}

bool DatabaseResult::IsNull(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL;
}

bool DatabaseResult::IsNullStr(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL;
}

int8 DatabaseResult::GetInt8(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? 0 : atoi(value);
}

int8 DatabaseResult::GetInt8Str(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? 0 : atoi(value);
}

sint8 DatabaseResult::GetSInt8(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? 0 : atoi(value);
}

sint8 DatabaseResult::GetSInt8Str(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? 0 : atoi(value);
}

int16 DatabaseResult::GetInt16(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? 0 : atoi(value);
}

int16 DatabaseResult::GetInt16Str(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? 0 : atoi(value);
}

sint16 DatabaseResult::GetSInt16(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? 0 : atoi(value);
}

sint16 DatabaseResult::GetSInt16Str(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? 0 : atoi(value);
}

int32 DatabaseResult::GetInt32(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? 0U : strtoul(value, NULL, 10);
}

int32 DatabaseResult::GetInt32Str(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? 0U : strtoul(value, NULL, 10);
}

sint32 DatabaseResult::GetSInt32(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? 0 : atoi(value);
}

sint32 DatabaseResult::GetSInt32Str(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? 0 : atoi(value);
}

uint64 DatabaseResult::GetInt64(unsigned int index) {
	const char *value = GetFieldValue(index);
#ifdef _WIN32
	return value == NULL ? 0UL : _strtoui64(value, NULL, 10);
#else
	return value == NULL ? 0UL : strtoull(value, NULL, 10);
#endif
}

uint64 DatabaseResult::GetInt64Str(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
#ifdef _WIN32
	return value == NULL ? 0UL : _strtoui64(value, NULL, 10);
#else
	return value == NULL ? 0UL : strtoull(value, NULL, 10);
#endif
}

sint64 DatabaseResult::GetSInt64(unsigned int index) {
	const char *value = GetFieldValue(index);
#ifdef _WIN32
	return value == NULL ? 0L : _strtoi64(value, NULL, 10);
#else
	return value == NULL ? 0L : strtoll(value, NULL, 10);
#endif
}

sint64 DatabaseResult::GetSInt64Str(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
#ifdef _WIN32
	return value == NULL ? 0L : _strtoi64(value, NULL, 10);
#else
	return value == NULL ? 0L : strtoll(value, NULL, 10);
#endif
}

float DatabaseResult::GetFloat(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? 0.0F : atof(value);
}

float DatabaseResult::GetFloatStr(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? 0.0F : atof(value);
}

char DatabaseResult::GetChar(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? '\0' : value[0];
}

char DatabaseResult::GetCharStr(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? '\0' : value[0];
}

const char * DatabaseResult::GetString(unsigned int index) {
	const char *value = GetFieldValue(index);
	return value == NULL ? empty_str : value;
}

const char * DatabaseResult::GetStringStr(const char *field_name) {
	const char *value = GetFieldValueStr(field_name);
	return value == NULL ? empty_str : value;
}
