/*Copyright © 2017 Jason Ekstrand

* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*
*/
#ifndef _RWNX_RBTREE_H
#define _RWNX_RBTREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "rwnx_types.h"

struct rb_node {
    uintptr_t parent;
    struct rb_node *left;
    struct rb_node *right;
};

struct rb_tree {
    struct rb_node *root;
};

#define	rb_entry(ptr, type, member) container_of(ptr, type, member)

static inline struct rb_node *
rb_node_parent(struct rb_node *n)
{
    return (struct rb_node *)(n->parent & ~(uintptr_t)1);
}

static bool
rb_node_is_black(struct rb_node *n)
{
    return (n == NULL) || (n->parent & 1);
}

static bool
rb_node_is_red(struct rb_node *n)
{
    return !rb_node_is_black(n);
}

static void
rb_node_set_black(struct rb_node *n)
{
    n->parent |= 1;
}

static void
rb_node_set_red(struct rb_node *n)
{
    n->parent &= ~1ull;
}

static void
rb_node_set_parent(struct rb_node *n, struct rb_node *p)
{
    n->parent = (n->parent & 1) | (uintptr_t)p;
}

static inline void
rb_tree_link_node(struct rb_node *node, struct rb_node *parent,
                  struct rb_node **rb_link)
{
    rb_node_set_parent(node, parent);
    rb_node_set_red(node);

    node->left = NULL;
    node->right = NULL;

    *rb_link = node;
}

static struct rb_node *
rb_node_minimum(struct rb_node *node)
{
    while (node->left)
        node = node->left;
    return node;
}

static void
rb_node_copy_color(struct rb_node *dst, struct rb_node *src)
{
    dst->parent = (dst->parent & ~1ull) | (src->parent & 1);
}

static void
rb_tree_splice(struct rb_tree *T, struct rb_node *u, struct rb_node *v)
{
    if (!u)
        return;
    struct rb_node *p = rb_node_parent(u);
    if (p == NULL) {
        if (T->root != u)
            return;
        T->root = v;
    } else if (u == p->left) {
        p->left = v;
    } else {
        if (u != p->right)
            return;
        p->right = v;
    }
    if (v)
        rb_node_set_parent(v, p);
}

static void
rb_tree_rotate_left(struct rb_tree *T, struct rb_node *x)
{
    if (!x || !x->right)
        return;

    struct rb_node *y = x->right;
    x->right = y->left;
    if (y->left)
        rb_node_set_parent(y->left, x);
    rb_tree_splice(T, x, y);
    y->left = x;
    rb_node_set_parent(x, y);
}

static void
rb_tree_rotate_right(struct rb_tree *T, struct rb_node *y)
{
    if (!y || !y->left)
        return;

    struct rb_node *x = y->left;
    y->left = x->right;
    if (x->right)
        rb_node_set_parent(x->right, y);
    rb_tree_splice(T, y, x);
    x->right = y;
    rb_node_set_parent(y, x);
}

static inline void
rb_tree_insert_color(struct rb_node *z, struct rb_tree *T)
{
    struct rb_node *p, *g;

    rb_node_set_red(z);
    while ((p = rb_node_parent(z)) && rb_node_is_red(p)) {

        g = rb_node_parent(p);
        if (!g)
            return;

        if (p == g->left) {
            struct rb_node *u = g->right;

            if (rb_node_is_red(u)) {
                rb_node_set_black(p);
                rb_node_set_black(u);
                rb_node_set_red(g);
                z = g;
            } else {
                if (z == p->right) {
                    z = p;
                    rb_tree_rotate_left(T, z);
                    p = rb_node_parent(z);
                }
                rb_node_set_black(p);
                rb_node_set_red(g);
                rb_tree_rotate_right(T, g);
            }
        } else {
            struct rb_node *u = g->left;
            if (rb_node_is_red(u)) {
                rb_node_set_black(p);
                rb_node_set_black(u);
                rb_node_set_red(g);
                z = g;
            } else {
                if (z == p->left) {
                    z = p;
                    rb_tree_rotate_right(T, z);
                    p = rb_node_parent(z);
                }
                rb_node_set_black(p);
                rb_node_set_red(g);
                rb_tree_rotate_left(T, g);
            }
        }
    }

    if (T->root) {
        rb_node_set_black(T->root);
    }
}

static inline void
rb_tree_remove(struct rb_node *z, struct rb_tree *T)
{
    struct rb_node *x, *x_p;
    struct rb_node *y = z;
    bool y_was_black = rb_node_is_black(y);
    if (z->left == NULL) {
        x = z->right;
        x_p = rb_node_parent(z);
        rb_tree_splice(T, z, x);
    } else if (z->right == NULL) {
        x = z->left;
        x_p = rb_node_parent(z);
        rb_tree_splice(T, z, x);
    } else {
        y = rb_node_minimum(z->right);
        y_was_black = rb_node_is_black(y);

        x = y->right;
        if (rb_node_parent(y) == z) {
            x_p = y;
        } else {
            x_p = rb_node_parent(y);
            rb_tree_splice(T, y, x);
            y->right = z->right;
            rb_node_set_parent(y->right, y);
        }
        if (y->left != NULL)
            return;

        rb_tree_splice(T, z, y);
        y->left = z->left;
        rb_node_set_parent(y->left, y);
        rb_node_copy_color(y, z);
    }
    if (x_p != NULL && x != x_p->left && x != x_p->right)
        return;

    if (!y_was_black)
        return;

    while (x != T->root && rb_node_is_black(x)) {
        if (x == x_p->left) {
            struct rb_node *w = x_p->right;
            if (rb_node_is_red(w)) {
                rb_node_set_black(w);
                rb_node_set_red(x_p);
                rb_tree_rotate_left(T, x_p);
                if (x != x_p->left)
                    return;
                w = x_p->right;
            }
            if (rb_node_is_black(w->left) && rb_node_is_black(w->right)) {
                rb_node_set_red(w);
                x = x_p;
            } else {
                if (rb_node_is_black(w->right)) {
                    rb_node_set_black(w->left);
                    rb_node_set_red(w);
                    rb_tree_rotate_right(T, w);
                    w = x_p->right;
                }
                rb_node_copy_color(w, x_p);
                rb_node_set_black(x_p);
                rb_node_set_black(w->right);
                rb_tree_rotate_left(T, x_p);
                x = T->root;
            }
        } else {
            struct rb_node *w = x_p->left;
            if (rb_node_is_red(w)) {
                rb_node_set_black(w);
                rb_node_set_red(x_p);
                rb_tree_rotate_right(T, x_p);
                if (x != x_p->right)
                    return;
                w = x_p->left;
            }
            if (rb_node_is_black(w->right) && rb_node_is_black(w->left)) {
                rb_node_set_red(w);
                x = x_p;
            } else {
                if (rb_node_is_black(w->left)) {
                    rb_node_set_black(w->right);
                    rb_node_set_red(w);
                    rb_tree_rotate_left(T, w);
                    w = x_p->left;
                }
                rb_node_copy_color(w, x_p);
                rb_node_set_black(x_p);
                rb_node_set_black(w->left);
                rb_tree_rotate_right(T, x_p);
                x = T->root;
            }
        }
        x_p = rb_node_parent(x);
    }
    if (x)
        rb_node_set_black(x);
}

#endif /* _RWNX_RBTREE_H */
