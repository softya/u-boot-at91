/*
 * (C) Copyright 2012
 * Joe Hershberger, National Instruments, joe.hershberger@ni.com
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/string.h>
#include <linux/ctype.h>

#include <common.h>
#include <environment.h>

#ifdef CONFIG_CMD_NET
#define ENV_FLAGS_NET_VARTYPE_REPS "im"
#else
#define ENV_FLAGS_NET_VARTYPE_REPS ""
#endif

static const char env_flags_vartype_rep[] = "sdxb" ENV_FLAGS_NET_VARTYPE_REPS;

/*
 * Parse the flags string from a .flags attribute list into the vartype enum.
 */
enum env_flags_vartype env_flags_parse_vartype(const char *flags)
{
	char *type;

	if (strlen(flags) <= ENV_FLAGS_VARTYPE_LOC)
		return env_flags_vartype_string;

	type = strchr(env_flags_vartype_rep,
		flags[ENV_FLAGS_VARTYPE_LOC]);

	if (type != NULL)
		return (enum env_flags_vartype)
			(type - &env_flags_vartype_rep[0]);

	printf("## Warning: Unknown environment variable type '%c'\n",
		flags[ENV_FLAGS_VARTYPE_LOC]);
	return env_flags_vartype_string;
}

static inline int is_hex_prefix(const char *value)
{
	return value[0] == '0' && (value[1] == 'x' || value[1] == 'X');
}

static void skip_num(int hex, const char *value, const char **end,
	int max_digits)
{
	int i;

	if (hex && is_hex_prefix(value))
		value += 2;

	for (i = max_digits; i != 0; i--) {
		if (hex && !isxdigit(*value))
			break;
		if (!hex && !isdigit(*value))
			break;
		value++;
	}
	if (end != NULL)
		*end = value;
}

/*
 * Based on the declared type enum, validate that the value string complies
 * with that format
 */
static int _env_flags_validate_type(const char *value,
	enum env_flags_vartype type)
{
	const char *end;
#ifdef CONFIG_CMD_NET
	const char *cur;
	int i;
#endif

	switch (type) {
	case env_flags_vartype_string:
		break;
	case env_flags_vartype_decimal:
		skip_num(0, value, &end, -1);
		if (*end != '\0')
			return -1;
		break;
	case env_flags_vartype_hex:
		skip_num(1, value, &end, -1);
		if (*end != '\0')
			return -1;
		if (value + 2 == end && is_hex_prefix(value))
			return -1;
		break;
	case env_flags_vartype_bool:
		if (value[0] != '1' && value[0] != 'y' && value[0] != 't' &&
		    value[0] != 'Y' && value[0] != 'T' &&
		    value[0] != '0' && value[0] != 'n' && value[0] != 'f' &&
		    value[0] != 'N' && value[0] != 'F')
			return -1;
		if (value[1] != '\0')
			return -1;
		break;
#ifdef CONFIG_CMD_NET
	case env_flags_vartype_ipaddr:
		cur = value;
		for (i = 0; i < 4; i++) {
			skip_num(0, cur, &end, 3);
			if (cur == end)
				return -1;
			if (i != 3 && *end != '.')
				return -1;
			if (i == 3 && *end != '\0')
				return -1;
			cur = end + 1;
		}
		break;
	case env_flags_vartype_macaddr:
		cur = value;
		for (i = 0; i < 6; i++) {
			skip_num(1, cur, &end, 2);
			if (cur == end)
				return -1;
			if (cur + 2 == end && is_hex_prefix(cur))
				return -1;
			if (i != 5 && *end != ':')
				return -1;
			if (i == 5 && *end != '\0')
				return -1;
			cur = end + 1;
		}
		break;
#endif
	case env_flags_vartype_end:
		return -1;
	}

	/* OK */
	return 0;
}

/*
 * Look for flags in a provided list and failing that the static list
 */
static inline int env_flags_lookup(const char *flags_list, const char *name,
	char *flags)
{
	int ret = 1;

	if (!flags)
		/* bad parameter */
		return -1;

	/* try the env first */
	if (flags_list)
		ret = env_attr_lookup(flags_list, name, flags);

	if (ret != 0)
		/* if not found in the env, look in the static list */
		ret = env_attr_lookup(ENV_FLAGS_LIST_STATIC, name, flags);

	return ret;
}

/*
 * Parse the flag charachters from the .flags attribute list into the binary
 * form to be stored in the environment entry->flags field.
 */
static int env_parse_flags_to_bin(const char *flags)
{
	return env_flags_parse_vartype(flags) & ENV_FLAGS_VARTYPE_BIN_MASK;
}

/*
 * Look for possible flags for a newly added variable
 * This is called specifically when the variable did not exist in the hash
 * previously, so the blanket update did not find this variable.
 */
void env_flags_init(ENTRY *var_entry)
{
	const char *var_name = var_entry->key;
	const char *flags_list = getenv(ENV_FLAGS_VAR);
	char flags[ENV_FLAGS_ATTR_MAX_LEN + 1] = "";
	int ret = 1;

	/* look in the ".flags" and static for a reference to this variable */
	ret = env_flags_lookup(flags_list, var_name, flags);

	/* if any flags were found, set the binary form to the entry */
	if (!ret && strlen(flags))
		var_entry->flags = env_parse_flags_to_bin(flags);
}

/*
 * Called on each existing env var prior to the blanket update since removing
 * a flag in the flag list should remove its flags.
 */
static int clear_flags(ENTRY *entry)
{
	entry->flags = 0;

	return 0;
}

/*
 * Call for each element in the list that defines flags for a variable
 */
static int set_flags(const char *name, const char *value)
{
	ENTRY e, *ep;

	e.key	= name;
	e.data	= NULL;
	hsearch_r(e, FIND, &ep, &env_htab, 0);

	/* does the env variable actually exist? */
	if (ep != NULL) {
		/* the flag list is empty, so clear the flags */
		if (value == NULL || strlen(value) == 0)
			ep->flags = 0;
		else
			/* assign the requested flags */
			ep->flags = env_parse_flags_to_bin(value);
	}

	return 0;
}

static int on_flags(const char *name, const char *value, enum env_op op,
	int flags)
{
	/* remove all flags */
	hwalk_r(&env_htab, clear_flags);

	/* configure any static flags */
	env_attr_walk(ENV_FLAGS_LIST_STATIC, set_flags);
	/* configure any dynamic flags */
	env_attr_walk(value, set_flags);

	return 0;
}
U_BOOT_ENV_CALLBACK(flags, on_flags);

/*
 * Perform consistency checking before creating, overwriting, or deleting an
 * environment variable. Called as a callback function by hsearch_r() and
 * hdelete_r(). Returns 0 in case of success, 1 in case of failure.
 * When (flag & H_FORCE) is set, do not print out any error message and force
 * overwriting of write-once variables.
 */

int env_flags_validate(const ENTRY *item, const char *newval, enum env_op op,
	int flag)
{
	const char *name;
#if !defined(CONFIG_ENV_OVERWRITE) && defined(CONFIG_OVERWRITE_ETHADDR_ONCE) \
&& defined(CONFIG_ETHADDR)
	const char *oldval = NULL;

	if (op != env_op_create)
		oldval = item->data;
#endif

	name = item->key;

	/* Default value for NULL to protect string-manipulating functions */
	newval = newval ? : "";

#ifndef CONFIG_ENV_OVERWRITE
	/*
	 * Some variables like "ethaddr" and "serial#" can be set only once and
	 * cannot be deleted, unless CONFIG_ENV_OVERWRITE is defined.
	 */
	if (op != env_op_create &&		/* variable exists */
		(flag & H_FORCE) == 0) {	/* and we are not forced */
		if (strcmp(name, "serial#") == 0 ||
		    (strcmp(name, "ethaddr") == 0
#if defined(CONFIG_OVERWRITE_ETHADDR_ONCE) && defined(CONFIG_ETHADDR)
		     && strcmp(oldval, __stringify(CONFIG_ETHADDR)) != 0
#endif	/* CONFIG_OVERWRITE_ETHADDR_ONCE && CONFIG_ETHADDR */
			)) {
			printf("Can't overwrite \"%s\"\n", name);
			return 1;
		}
	}
#endif

	/* validate the value to match the variable type */
	if (op != env_op_delete) {
		enum env_flags_vartype type = (enum env_flags_vartype)
			(ENV_FLAGS_VARTYPE_BIN_MASK & item->flags);

		if (_env_flags_validate_type(newval, type) < 0) {
			printf("## Error: flags type check failure for "
				"\"%s\" <= \"%s\" (type: %c)\n",
				name, newval, env_flags_vartype_rep[type]);
			return -1;
		}
	}

	return 0;
}