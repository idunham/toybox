/* vi: set sw=4 ts=4 :*/
/* dirtree.c - Functions for dealing with directory trees.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 */

#include "toys.h"

// Create a dirtree node from a path, with stat and symlink info.

struct dirtree *dirtree_add_node(int dirfd, char *name)
{
	struct dirtree *dt = NULL;
	struct stat st;
	char buf[4096];
	int len = 0, linklen = 0;

	if (name) {
		if (fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW)) goto error;
		if (S_ISLNK(st.st_mode)) {
			if (0>(linklen = readlinkat(dirfd, name, buf, 4095))) goto error;
			buf[linklen++]=0;
		}
		len = strlen(name);
	}
   	dt = xzalloc((len = sizeof(struct dirtree)+len+1)+linklen);
	if (name) {
		memcpy(&(dt->st), &st, sizeof(struct stat));
		strcpy(dt->name, name);

		if (linklen) {
			dt->symlink = memcpy(len+(char *)dt, buf, linklen);
			dt->data = --linklen;
		}
	}

	return dt;

error:
	perror_msg("%s",name);
	free(dt);
	return 0;
}

// Return path to this node.

char *dirtree_path(struct dirtree *node, int *plen)
{
	char *path;
	int len;

	if (!node || !node->name) return xmalloc(*plen);

	len = (plen ? *plen : 0) + strlen(node->name)+1;
	path = dirtree_path(node->parent, &len);
	len = plen ? *plen : 0;
	if (len) path[len++]='/';
	strcpy(path+len, node->name);

	return path;
}

// Default callback, filters out "." and "..".

int dirtree_isdotdot(struct dirtree *catch)
{
	// Should we skip "." and ".."?
	if (catch->name[0]=='.' && (!catch->name[1] ||
			(catch->name[1]=='.' && !catch->name[2])))
				return DIRTREE_NOSAVE|DIRTREE_NORECURSE;

	return 0;
}

// Handle callback for a node in the tree. Returns saved node(s) or NULL.
//
// By default, allocates a tree of struct dirtree, not following symlinks
// If callback==NULL, or callback always returns 0, allocate tree of struct
// dirtree and return root of tree.  Otherwise call callback(node) on each hit, free
// structures after use, and return NULL.
//

struct dirtree *handle_callback(struct dirtree *new,
					int (*callback)(struct dirtree *node))
{
	int flags;

	if (!callback) callback = dirtree_isdotdot;

	flags = callback(new);
	if (S_ISDIR(new->st.st_mode)) {
		if (!(flags & DIRTREE_NORECURSE)) {
			new->data = openat(new->data, new->name, 0);
			dirtree_recurse(new, callback);
		}
		new->data = -1;
		if (flags & DIRTREE_COMEAGAIN) flags = callback(new);
	}
	// If this had children, it was callback's job to free them already.
	if (flags & DIRTREE_NOSAVE) {
		free(new);
		new = NULL;
	}

	return (flags & DIRTREE_ABORT)==DIRTREE_ABORT ? DIRTREE_ABORTVAL : new;
}

// Recursively read/process children of directory node (with dirfd in data),
// filtering through callback().

void dirtree_recurse(struct dirtree *node,
					int (*callback)(struct dirtree *node))
{
	struct dirtree *new, **ddt = &(node->child);
	struct dirent *entry;
	DIR *dir;
	int dirfd;

	if (!(dir = fdopendir(node->data))) {
		char *path = dirtree_path(node, 0);
		perror_msg("No %s", path);
		free(path);
		close(node->data);
	}
	// Dunno if I really need to do this, but the fdopendir man page insists
	dirfd = xdup(node->data);

	// The extra parentheses are to shut the stupid compiler up.
	while ((entry = readdir(dir))) {
		if (!(new = dirtree_add_node(dirfd, entry->d_name))) continue;
		new->parent = node;
		new = handle_callback(new, callback);
		if (new == DIRTREE_ABORTVAL) break;
		if (new) {
			*ddt = new;
			ddt = &((*ddt)->next);
		}
	}

	closedir(dir);
	close(dirfd);
}

// Create dirtree from path, using callback to filter nodes.
// If callback == NULL allocate a tree of struct dirtree nodes and return
// pointer to root node.

struct dirtree *dirtree_read(char *path, int (*callback)(struct dirtree *node))
{
	int fd = open(".", 0);
	struct dirtree *root = dirtree_add_node(fd, path);
	root->data = fd;

	return handle_callback(root, callback);
}
