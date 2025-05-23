/*****************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *****************************************************************************/

#include "common.h"
#include "../check_disk.d/utils_disk.h"
#include "../../tap/tap.h"
#include "regex.h"

void np_test_mount_entry_regex(struct mount_entry *dummy_mount_list, char *regstr, int cflags, int expect, char *desc);

int main(int argc, char **argv) {
	plan_tests(35);

	struct name_list *exclude_filesystem = NULL;
	ok(np_find_name(exclude_filesystem, "/var/log") == false, "/var/log not in list");
	np_add_name(&exclude_filesystem, "/var/log");
	ok(np_find_name(exclude_filesystem, "/var/log") == true, "is in list now");
	ok(np_find_name(exclude_filesystem, "/home") == false, "/home not in list");
	np_add_name(&exclude_filesystem, "/home");
	ok(np_find_name(exclude_filesystem, "/home") == true, "is in list now");
	ok(np_find_name(exclude_filesystem, "/var/log") == true, "/var/log still in list");

	struct name_list *exclude_fstype = NULL;
	ok(np_find_name(exclude_fstype, "iso9660") == false, "iso9660 not in list");
	np_add_name(&exclude_fstype, "iso9660");
	ok(np_find_name(exclude_fstype, "iso9660") == true, "is in list now");

	ok(np_find_name(exclude_filesystem, "iso9660") == false, "Make sure no clashing in variables");

	/*
	for (temp_name = exclude_filesystem; temp_name; temp_name = temp_name->next) {
		printf("Name: %s\n", temp_name->name);
	}
	*/

	struct mount_entry *dummy_mount_list;
	struct mount_entry **mtail = &dummy_mount_list;
	struct mount_entry *me = (struct mount_entry *)malloc(sizeof *me);
	me->me_devname = strdup("/dev/c0t0d0s0");
	me->me_mountdir = strdup("/");
	*mtail = me;
	mtail = &me->me_next;

	me = (struct mount_entry *)malloc(sizeof *me);
	me->me_devname = strdup("/dev/c1t0d1s0");
	me->me_mountdir = strdup("/var");
	*mtail = me;
	mtail = &me->me_next;

	me = (struct mount_entry *)malloc(sizeof *me);
	me->me_devname = strdup("/dev/c2t0d0s0");
	me->me_mountdir = strdup("/home");
	*mtail = me;
	mtail = &me->me_next;

	int cflags = REG_NOSUB | REG_EXTENDED;
	np_test_mount_entry_regex(dummy_mount_list, strdup("/"), cflags, 3, strdup("a"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("/dev"), cflags, 3, strdup("regex on dev names:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("/foo"), cflags, 0, strdup("regex on non existent dev/path:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("/Foo"), cflags | REG_ICASE, 0, strdup("regi on non existent dev/path:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("/c.t0"), cflags, 3, strdup("partial devname regex match:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("c0t0"), cflags, 1, strdup("partial devname regex match:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("C0t0"), cflags | REG_ICASE, 1, strdup("partial devname regi match:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("home"), cflags, 1, strdup("partial pathname regex match:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("hOme"), cflags | REG_ICASE, 1, strdup("partial pathname regi match:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("(/home)|(/var)"), cflags, 2, strdup("grouped regex pathname match:"));
	np_test_mount_entry_regex(dummy_mount_list, strdup("(/homE)|(/Var)"), cflags | REG_ICASE, 2, strdup("grouped regi pathname match:"));

	filesystem_list test_paths = filesystem_list_init();
	mp_int_fs_list_append(&test_paths, "/home/groups");
	mp_int_fs_list_append(&test_paths, "/var");
	mp_int_fs_list_append(&test_paths, "/tmp");
	mp_int_fs_list_append(&test_paths, "/home/tonvoon");
	mp_int_fs_list_append(&test_paths, "/dev/c2t0d0s0");
	ok(test_paths.length == 5, "List counter works correctly with appends");

	mp_int_fs_list_set_best_match(test_paths, dummy_mount_list, false);
	for (parameter_list_elem *p = test_paths.first; p; p = mp_int_fs_list_get_next(p)) {
		struct mount_entry *temp_me;
		temp_me = p->best_match;
		if (!strcmp(p->name, "/home/groups")) {
			ok(temp_me && !strcmp(temp_me->me_mountdir, "/home"), "/home/groups got right best match: /home");
		} else if (!strcmp(p->name, "/var")) {
			ok(temp_me && !strcmp(temp_me->me_mountdir, "/var"), "/var got right best match: /var");
		} else if (!strcmp(p->name, "/tmp")) {
			ok(temp_me && !strcmp(temp_me->me_mountdir, "/"), "/tmp got right best match: /");
		} else if (!strcmp(p->name, "/home/tonvoon")) {
			ok(temp_me && !strcmp(temp_me->me_mountdir, "/home"), "/home/tonvoon got right best match: /home");
		} else if (!strcmp(p->name, "/dev/c2t0d0s0")) {
			ok(temp_me && !strcmp(temp_me->me_devname, "/dev/c2t0d0s0"), "/dev/c2t0d0s0 got right best match: /dev/c2t0d0s0");
		}
	}

	for (parameter_list_elem *p = test_paths.first; p; p = mp_int_fs_list_get_next(p)) {
		mp_int_fs_list_del(&test_paths, p);
	}
	ok(test_paths.length == 0, "List delete sets counter properly");

	mp_int_fs_list_append(&test_paths, "/home/groups");
	mp_int_fs_list_append(&test_paths, "/var");
	mp_int_fs_list_append(&test_paths, "/tmp");
	mp_int_fs_list_append(&test_paths, "/home/tonvoon");
	mp_int_fs_list_append(&test_paths, "/home");

	mp_int_fs_list_set_best_match(test_paths, dummy_mount_list, true);
	for (parameter_list_elem *p = test_paths.first; p; p = mp_int_fs_list_get_next(p)) {
		if (!strcmp(p->name, "/home/groups")) {
			ok(!p->best_match, "/home/groups correctly not found");
		} else if (!strcmp(p->name, "/var")) {
			ok(p->best_match, "/var found");
		} else if (!strcmp(p->name, "/tmp")) {
			ok(!p->best_match, "/tmp correctly not found");
		} else if (!strcmp(p->name, "/home/tonvoon")) {
			ok(!p->best_match, "/home/tonvoon not found");
		} else if (!strcmp(p->name, "/home")) {
			ok(p->best_match, "/home found");
		}
	}

	bool found = false;
	/* test deleting first element in paths */
	mp_int_fs_list_del(&test_paths, NULL);
	for (parameter_list_elem *p = test_paths.first; p; p = mp_int_fs_list_get_next(p)) {
		if (!strcmp(p->name, "/home/groups")) {
			found = true;
		}
	}
	ok(!found, "first element successfully deleted");
	found = false;

	parameter_list_elem *prev = NULL;
	parameter_list_elem *p = NULL;
	for (parameter_list_elem *path = test_paths.first; path; path = mp_int_fs_list_get_next(path)) {
		if (!strcmp(path->name, "/tmp")) {
			mp_int_fs_list_del(&test_paths, path);
		}
		p = path;
	}

	parameter_list_elem *last = NULL;
	for (parameter_list_elem *path = test_paths.first; path; path = mp_int_fs_list_get_next(path)) {
		if (!strcmp(path->name, "/tmp")) {
			found = true;
		}
		if (path->next) {
			prev = path;
		} else {
			last = path;
		}
	}
	ok(!found, "/tmp element successfully deleted");

	int count = 0;
	mp_int_fs_list_del(&test_paths, p);
	for (p = test_paths.first; p; p = p->next) {
		if (!strcmp(p->name, "/home")) {
			found = true;
		}
		last = p;
		count++;
	}
	ok(!found, "last (/home) element successfully deleted");
	ok(count == 2, "two elements remaining");

	return exit_status();
}

void np_test_mount_entry_regex(struct mount_entry *dummy_mount_list, char *regstr, int cflags, int expect, char *desc) {
	regex_t regex;
	if (regcomp(&regex, regstr, cflags) == 0) {
		int matches = 0;
		for (struct mount_entry *me = dummy_mount_list; me; me = me->me_next) {
			if (np_regex_match_mount_entry(me, &regex)) {
				matches++;
			}
		}
		ok(matches == expect, "%s '%s' matched %i/3 entries. ok: %i/3", desc, regstr, expect, matches);

	} else {
		ok(false, "regex '%s' not compilable", regstr);
	}
}
