#include "common.h"

extern void free_track(TacticalTrack* track);

// 1-1. Create a new B-Tree Node
BTreeNode* create_btree_node(bool is_leaf) {
    BTreeNode* node = (BTreeNode*)malloc(sizeof(BTreeNode));
    if (node == NULL) {
        printf("[ERROR] Memory allocation failed for B-Tree Node.\n");
        exit(1);
    }
    
    node->is_leaf = is_leaf;
    node->num_keys = 0;
    
    // Initialize children to NULL for safety
    for (int i = 0; i < MAX_CHILDREN; i++) {
        node->children[i] = NULL;
    }
    // Initialize tracks to NULL
    for (int i = 0; i < MAX_KEYS; i++) {
        node->tracks[i] = NULL;
    }

    return node;
}

// 1-2. Search for a Track by ID (O(log N) Speed)
TacticalTrack* search_btree(BTreeNode* node, int track_id) {
    if (node == NULL) return NULL;

    // Find the first key greater than or equal to track_id
    int i = 0;
    while (i < node->num_keys && track_id > node->tracks[i]->track_id) {
        i++;
    }

    // Case 1: Found the track in this node
    if (i < node->num_keys && node->tracks[i]->track_id == track_id) {
        return node->tracks[i];
    }

    // Case 2: If leaf, track is not in the tree
    if (node->is_leaf) {
        return NULL;
    }

    // Case 3: Recurse to the appropriate child
    return search_btree(node->children[i], track_id);
}

/* =================================================================
   PART 2: Insertion Logic (The Core Engine)
   - Handles node splitting when full
================================================================= */

// 2-1. Split Child: Splits a full child node (y) into two (y and z)
void split_child(BTreeNode* parent, int i, BTreeNode* child_full) {
    BTreeNode* z = create_btree_node(child_full->is_leaf);
    z->num_keys = BTREE_T - 1;

    // Move the last (t-1) keys of child_full to z
    for (int j = 0; j < BTREE_T - 1; j++) {
        z->tracks[j] = child_full->tracks[j + BTREE_T];
    }

    // If not leaf, move children pointers too
    if (!child_full->is_leaf) {
        for (int j = 0; j < BTREE_T; j++) {
            z->children[j] = child_full->children[j + BTREE_T];
        }
    }

    child_full->num_keys = BTREE_T - 1;

    // Shift parent's children to make room for new child z
    for (int j = parent->num_keys; j >= i + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[i + 1] = z;

    // Shift parent's keys to make room for the median key
    for (int j = parent->num_keys - 1; j >= i; j--) {
        parent->tracks[j + 1] = parent->tracks[j];
    }

    // Move the median key from child_full to parent
    parent->tracks[i] = child_full->tracks[BTREE_T - 1];
    parent->num_keys++;
}

// 2-2. Insert into Non-Full Node
void insert_non_full(BTreeNode* node, TacticalTrack* track) {
    int i = node->num_keys - 1;

    if (node->is_leaf) {
        // Shift keys to find correct position (Sorting)
        while (i >= 0 && track->track_id < node->tracks[i]->track_id) {
            node->tracks[i + 1] = node->tracks[i];
            i--;
        }
        node->tracks[i + 1] = track;
        node->num_keys++;
        
        LOG_WAYPOINT("INSERT", track->track_id, "Inserted into B-Tree Leaf.");
    } else {
        // Find child to recurse
        while (i >= 0 && track->track_id < node->tracks[i]->track_id) {
            i--;
        }
        i++;

        // If child is full, split it first
        if (node->children[i]->num_keys == MAX_KEYS) {
            split_child(node, i, node->children[i]);
            if (track->track_id > node->tracks[i]->track_id) {
                i++;
            }
        }
        insert_non_full(node->children[i], track);
    }
}

// 2-3. Main Insert Function (Handles Root Splitting)
void insert_track(BTreeNode** root, TacticalTrack* track) {
    BTreeNode* r = *root;

    // If tree is empty, create root
    if (r == NULL) {
        *root = create_btree_node(true);
        (*root)->tracks[0] = track;
        (*root)->num_keys = 1;
        LOG_WAYPOINT("INSERT", track->track_id, "Root created & Track inserted.");
        return;
    }

    // If root is full, tree grows in height
    if (r->num_keys == MAX_KEYS) {
        BTreeNode* s = create_btree_node(false); // New root
        *root = s;
        s->children[0] = r;
        split_child(s, 0, r);
        insert_non_full(s, track);
        LOG_WAYPOINT("SPLIT", track->track_id, "Root split occurred (Tree Height +1).");
    } else {
        insert_non_full(r, track);
    }
}

/* =================================================================
   PART 3: Memory Management & Visualization
   - Prevents memory leaks
   - Visualizes tree structure
================================================================= */

// 3-1. Recursive Free (Garbage Collection)
void free_btree(BTreeNode* node) {
    if (node == NULL) return;

    // Recursively free all children first
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            free_btree(node->children[i]);
        }
    }

    // Free all tracks in this node
    for (int i = 0; i < node->num_keys; i++) {
        // We use free_track from track.c to clean up history linked lists too
        free_track(node->tracks[i]); 
    }

    // Finally, free the node itself
    free(node);
}

// 3-2. Visualize Tree Structure (DFS Traversal)
void print_btree(BTreeNode* node, int level) {
    if (node == NULL) return;

    printf("  [Lv.%d] ", level);
    for (int i = 0; i < node->num_keys; i++) {
        printf("[%d] ", node->tracks[i]->track_id);
    }
    printf("\n");

    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            print_btree(node->children[i], level + 1);
        }
    }
}