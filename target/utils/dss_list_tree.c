/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Judy.h>
#include <string.h>
#include <malloc.h>

#include "dragonfly.h"

#define DSS_LIST_MAX_KLEN (1024)

dss_hsl_ctx_t *dss_hsl_new_ctx(char *root_prefix, char *delim_str, list_item_cb list_cb)
{
	dss_hsl_ctx_t *hsl_ctx = (dss_hsl_ctx_t *)calloc(1, sizeof(dss_hsl_ctx_t));

	if(hsl_ctx) {
		hsl_ctx->root_prefix = strdup(root_prefix);
		hsl_ctx->delim_str = strdup(delim_str);

		hsl_ctx->lnode.leaf = 0;
		assert(list_cb);
		hsl_ctx->process_listing_item = list_cb;
		//hsl_ctx->lnode.tree_filled = 0;
		hsl_ctx->lnode.subtree = NULL;

		return hsl_ctx;
	} else {
		return NULL;
	}
}
int _dss_hsl_delete_key(dss_hsl_ctx_t *hctx, dss_hslist_node_t *tnode, char *tok, char **saveptr)
{

	char *token_next;

	Word_t *value;
	int rc;

	token_next = strtok_r(NULL, hctx->delim_str, saveptr);
	if(token_next) {
		dss_hslist_node_t *next_node;
		uint8_t key_str[DSS_LIST_MAX_KLEN + 1];

		//hctx->node_count--;
		//Find next node and call recursive delete
		value = (Word_t *) JudySLGet(tnode->subtree, tok, PJE0);//Get token node for next level

		if(value) {
			next_node = (dss_hslist_node_t *)*value;
			rc = _dss_hsl_delete_key(hctx, next_node, token_next, saveptr);
			if(rc == 0) {
				return 0;
			}

			strcpy(key_str, "");
			value = (Word_t *) JudySLNext(next_node->subtree, key_str, PJE0);
			if(value) {
				//Judy SL array not empty
				return 0;
			} else {
				JudySLFreeArray(&next_node->subtree, PJE0);
				next_node->subtree = NULL;
				free(next_node);

				hctx->node_count--;

				rc = JudySLDel(&tnode->subtree, tok, PJE0);
				assert(rc == 1);//Found Key should be deleted

				return 1;
			}

		} else {
			//Key not found
			return 0;
		}
	} else {
		dss_hslist_node_t *leaf_node;

		//Check leaf and delete entry
		value = (Word_t *) JudySLGet(tnode->subtree, tok, PJE0);

		if(value) {
			leaf_node = (dss_hslist_node_t *)*value;

			assert(leaf_node->leaf == 1);
			assert(leaf_node->subtree == NULL);

			free(leaf_node);

			hctx->node_count--;

			rc = JudySLDel(&tnode->subtree, tok, PJE0);
			assert(rc == 1);//Found Key should be deleted

			return 1;
		} else {
			//Key not found
			return 0;
		}

		return 0;
	}


}

int dss_hsl_delete(dss_hsl_ctx_t *hctx, const char *key)
{
	Word_t *value;
	uint8_t key_str[DSS_LIST_MAX_KLEN + 1];

	char *tok, *saveptr;

	dss_hslist_node_t *tnode;
	uint32_t depth = 0;

	if(strlen(key) > DSS_LIST_MAX_KLEN) return -1;

	strncpy((char *)key_str, key, DSS_LIST_MAX_KLEN);
	key_str[DSS_LIST_MAX_KLEN] = '\0';

	tnode = &hctx->lnode;
	//Fist delmiter
	tok =  strtok_r(key_str, hctx->delim_str, &saveptr);

	if(tok && !strcmp(hctx->root_prefix, tok)) {
		//Advance token - root prefix in not saved
		tok = strtok_r(NULL, hctx->delim_str, &saveptr);
	} else {
		return 0;
	}

	return _dss_hsl_delete_key(hctx, tnode, tok, &saveptr);
}

int dss_hsl_insert(dss_hsl_ctx_t *hctx, const char *key)
{
	Word_t *value;
	uint8_t key_str[DSS_LIST_MAX_KLEN + 1];

	char *tok, *saveptr;

	dss_hslist_node_t *tnode;
	uint32_t depth = 0;

	if(strlen(key) > DSS_LIST_MAX_KLEN) return -1;

	strncpy((char *)key_str, key, DSS_LIST_MAX_KLEN);
	key_str[DSS_LIST_MAX_KLEN] = '\0';

	tnode = &hctx->lnode;
	//Fist delmiter
	tok =  strtok_r(key_str, hctx->delim_str, &saveptr);

	if(tok && !strcmp(hctx->root_prefix, tok)) {
		//Advance token - root prefix in not saved
		tok = strtok_r(NULL, hctx->delim_str, &saveptr);
	} else {
		return 0;
	}

	while(tok) {

		//printf("Insert token %s at node %p depth %d isleaf:%d \n", tok, tnode, depth, tnode->leaf);

		value = (Word_t *)JudySLIns(&tnode->subtree, tok, PJE0);

		if(!*value) {
#if defined DSS_LIST_DEBUG_MEM_USE
			hctx->node_count++;
#endif
			*value = (Word_t) calloc(1, sizeof(dss_hslist_node_t));
		}

		tnode = (dss_hslist_node_t *)*value;

		tok = strtok_r(NULL, hctx->delim_str, &saveptr);

		if(tok) {
		//if(tok || key[strlen(key) - 1] == hctx->delim) {
			tnode->leaf = 0;
		} else {
			tnode->leaf = 1;
		}

		if(tok) depth++;//Root node is depth 0
	}

	if(hctx->tree_depth < depth) {
		hctx->tree_depth = depth;
	}

	return 0;
}

Word_t dss_hsl_list_all(dss_hslist_node_t *node);

dss_hsl_print_info(dss_hsl_ctx_t *hctx)
{
	//printf("JudySL mem usage word count %ld\n", JudyMallocMemUsed());
	printf("DSS hsl node count %ld\n", hctx->node_count);
	printf("DSS hsl tree dept %ld\n", hctx->tree_depth);
	printf("DSS hsl tree elment count  %ld\n", dss_hsl_list_all((dss_hslist_node_t *)&hctx->lnode));
}


Word_t dss_hsl_list_all(dss_hslist_node_t *node)
{
	Word_t *value;
	uint64_t leaf_count = 0;
	uint8_t list_str[DSS_LIST_MAX_KLEN + 1];

	if (node->leaf == 1) {
		return 1;
	}

	strcpy(list_str, "");

	value = (Word_t *) JudySLFirst(node->subtree, list_str, PJE0);

	while(value) {
		leaf_count += dss_hsl_list_all((dss_hslist_node_t *)*value);

		value = (Word_t *) JudySLNext(node->subtree, list_str, PJE0);
	}

	return leaf_count;
}

void dss_hsl_list(dss_hsl_ctx_t *hctx, const char *prefix, const char *start_key, void *listing_ctx)
{
	Word_t *value;
	uint8_t prefix_str[DSS_LIST_MAX_KLEN + 1];
	uint8_t list_str[DSS_LIST_MAX_KLEN + 1];

	char *tok, *saveptr;

	dss_hslist_node_t *tnode, *lnode;

	int rc;

	if(strlen(prefix) > DSS_LIST_MAX_KLEN) return;

	strncpy((char *)prefix_str, prefix, DSS_LIST_MAX_KLEN);
	prefix_str[DSS_LIST_MAX_KLEN] = '\0';

	tnode = &hctx->lnode;
	//Fist delmiter
	tok =  strtok_r(prefix_str, hctx->delim_str, &saveptr);

	if(tok && !strcmp(hctx->root_prefix, tok)) {
		//Advance token - root prefix in not saved
		tok = strtok_r(NULL, hctx->delim_str, &saveptr);
	}

	//printf("Prefix: %s\n", prefix);
	//Find Node
	while(tok) {
		value = (Word_t *) JudySLGet(tnode->subtree, tok, PJE0);

		if(!value) {
			//Entry not found
			printf("Coundn't find token %s\n", tok);
			return;
		}

		tnode = (dss_hslist_node_t *)*value;
		assert(tnode != NULL);

		tok = strtok_r(NULL, hctx->delim_str, &saveptr);

		if(!tok) {
			if(tnode->leaf == 1) {
				//printf("Not listing leaf str %s\n", prefix);
				tnode = lnode;
			}//Non leaf use node to list hierarchical entry
		}
	}

	//TODO: Support start key (could be leaf or non leaf from between a JudySL
	//Listing
	strcpy(list_str, "");

	value = (Word_t *) JudySLFirst(tnode->subtree, list_str, PJE0);

	if(value && start_key) {
		strcpy(list_str, start_key);
		value = (Word_t *) JudySLNext(tnode->subtree, list_str, PJE0);
	}

	while (value) {
		assert(*value);
		lnode = (dss_hslist_node_t *)*value;

		if(lnode->leaf) {
			rc = hctx->process_listing_item(listing_ctx, list_str, 1);
		} else {
			rc = hctx->process_listing_item(listing_ctx, list_str, 0);
		}

		if(rc != 0) {
			return;
		}

		value = (Word_t *) JudySLNext(tnode->subtree, list_str, PJE0);
	}

	return;

}
