/* 
    Zz Dynamic Parser Library
    Copyright (C) 1989 - I.N.F.N - S.Cabasino, P.S.Paolucci, G.M.Todesco

    The Zz Library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    The Zz Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*   $Id: avl.c,v 1.3 2002/05/09 17:23:49 kibun Exp $ */

#include <stdlib.h>
#define   AVL_C
#include "avl.h"


#define LOOP  for (;;)

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#define NIL              0
#define NILARG  ((void *)0)

typedef unsigned long  ulong;
typedef unsigned int   uint;
typedef unsigned short ushort;

typedef struct avl_node NODE;
typedef struct avl_path PATH;

#define MBR_KEY  (AVL_NODUP_MBR >> 1)
#define PTR_KEY  (AVL_NODUP_PTR >> 1)
#define STR_KEY  (AVL_NODUP_STR >> 1)
#define LNG_KEY  (AVL_NODUP_LNG >> 1)
#define INT_KEY  (AVL_NODUP_INT >> 1)
#define SHT_KEY  (AVL_NODUP_SHT >> 1)
#define ULN_KEY  (AVL_NODUP_ULN >> 1)
#define UIN_KEY  (AVL_NODUP_UIN >> 1)
#define USH_KEY  (AVL_NODUP_USH >> 1)
#define CHR_KEY  (AVL_NODUP_CHR >> 1)

#define USR_CMP  0
#define STR_CMP  1
#define VAL_CMP  2
#define COR_CMP  3

#define NODUP 0
#define DUP   1

#define USR_NODUP   (USR_CMP | NODUP << 2)
#define STR_NODUP   (STR_CMP | NODUP << 2)
#define VAL_NODUP   (VAL_CMP | NODUP << 2)
#define COR_NODUP   (COR_CMP | NODUP << 2)
#define USR_DUP     (USR_CMP | DUP   << 2)
#define STR_DUP     (STR_CMP | DUP   << 2)
#define VAL_DUP     (VAL_CMP | DUP   << 2)
#define COR_DUP     (COR_CMP | DUP   << 2)

/* keyinfo: bits 6-3: KEYTYPE, bit 2: DUP, bits 1-0: CMPTYPE */
/*                bits 6-2: TREETYPE, bits 2-0: LOCTYPE      */

#define KEYTYPE(keyinfo)     ((keyinfo) >> 3)
#define TREETYPE(keyinfo)    ((keyinfo) >> 2)
#define CMPTYPE(keyinfo)     ((keyinfo) & 0x3)
#define LOCTYPE(keyinfo)     ((keyinfo) & 0x7)

#define MAXUSHORT (((unsigned short)(~0))>>1)

#define MINLONG ((long)~(((unsigned long)(~0L))>>1))

#define CORRECT(u) ((long)(u) + MINLONG)

#define SET_STRCMP(cmp, str1, str2) \
  { \
  register char *p1, *p2; \
  for (p1 = (str1), p2 = (str2); *p1 && *p1 == *p2; p1++, p2++) \
    ; \
  (cmp) = *p1 - *p2; \
  }

#define SET_PTRCMP(cmp, usrcmp, ptr1, ptr2) \
  { \
  if (usrcmp) \
    (cmp) = (*(usrcmp)) ((ptr1), (ptr2)); \
  else \
    SET_STRCMP ((cmp), (char *)(ptr1), (char *)(ptr2)) \
  }

#define ERROR    (-1)

#define BAL        0
#define LEFT       1
#define RIGHT      2
#define LEFTUNBAL  3
#define RIGHTUNBAL 4

#define LESS       3
#define SAME       4

#define NOTINS     0
#define INS        1
#define DEEPER     2

#define NODE_LIST  0
#define TREE_LIST  (NODE_LIST + ( (sizeof (NODE) != sizeof(TREE))?1:0 ) )
#define PATH_LIST  (TREE_LIST + 1)
#define FREE_LISTS (PATH_LIST + 1)

#define LONG_ALIGNED_SIZE(obj) ((sizeof(obj)+sizeof(long)-1)&~(sizeof(long)-1))

#define SIZEOF_NODE  LONG_ALIGNED_SIZE (NODE)
#define SIZEOF_TREE  LONG_ALIGNED_SIZE (TREE)
#define SIZEOF_PATH  LONG_ALIGNED_SIZE (PATH)
#define SIZEOF_LONG  sizeof (long)

#define PREALLOC_LONGS ((32768 - 16)   / SIZEOF_LONG)
#define PREALLOC_SIZE  (PREALLOC_LONGS * SIZEOF_LONG)
#define MALLOC_LONGS   ((32768 - 32)   / SIZEOF_LONG)
#define MALLOC_SIZE    (MALLOC_LONGS   * SIZEOF_LONG)

static long Prealloc[PREALLOC_LONGS];

static void *Free_List[FREE_LISTS]; /* init to 0 by C */
static char *Avail_Base = (char *)Prealloc;
static uint  Avail_Size = PREALLOC_SIZE;

/*===========================================================================*/

static void *new_memory (uint size)
{
  char *base;
  
  while (Avail_Size >= SIZEOF_NODE)
    {
      base = Avail_Base + (Avail_Size -= SIZEOF_NODE);
      *(void **)base = Free_List[NODE_LIST];
      Free_List[NODE_LIST] = (void *)base;
    }

  if ( (Avail_Base = (char *)malloc (MALLOC_SIZE)) )
    {
      Avail_Size = MALLOC_SIZE - size;
      
      return (void *)(Avail_Base + Avail_Size);
    }
  else
    {
      Avail_Size = 0;
      return NIL;
    }
}

#if 0
#warning "memmgr: original implementation"
/*
  this version seems to trigger a 'bug' of gcc-2.96-98 (RH7.2)
  maybe due to aliasing of this inlined code
 */
#define ALLOC_OBJ(ptr, ptr_type, size, list) \
  { \
  if (Free_List[list]) \
    { \
    (ptr) = (ptr_type) Free_List[list]; \
                       Free_List[list] = *(void **)(ptr); \
    } \
  else if (Avail_Size >= size) \
    (ptr) = (ptr_type) (Avail_Base + \
                       (Avail_Size -= size)); \
  else \
    (ptr) = (ptr_type) new_memory (size); \
  }

#define FREE_OBJ(ptr, list) \
  { \
  *(void **)(ptr) = Free_List[list]; \
                    Free_List[list] = (void *)(ptr); \
  }

#elif 1

//#warning "memmgr: mixed macro/inlinefun implementation"

static inline void* __alloc_obj(uint size, uint list)
{
  void* ptr=0;
  if (Free_List[list])
    {
      ptr = Free_List[list];
      Free_List[list] = *(void **)(ptr);
    }
  else if (Avail_Size >= size)
    ptr = (Avail_Base + (Avail_Size -= size));
  else
    ptr = new_memory (size);
  return ptr;
}

static inline void __free_obj(void* ptr, uint list)
{
  *(void **)(ptr) = Free_List[list];
  Free_List[list] = (void *)(ptr);
}

#define ALLOC_OBJ(ptr, ptr_type, size, list) \
  do { (ptr) = (ptr_type) __alloc_obj(size,list); } while(0)

#define FREE_OBJ(ptr, list) \
  do { __free_obj(ptr,list); } while(0)

#else

#warning "memmgr: plain malloc/free implementation"

#define ALLOC_OBJ(ptr, ptr_type, size, list) \
  do {(ptr) = (ptr_type) malloc (size);} while(0)

#define FREE_OBJ(ptr, list) \
  do { free(ptr); } while(0)

#endif

#define ALLOC_NODE(node)  ALLOC_OBJ (node, NODE *, SIZEOF_NODE, NODE_LIST)
#define ALLOC_TREE(tree)  ALLOC_OBJ (tree, TREE *, SIZEOF_TREE, TREE_LIST)
#define ALLOC_PATH(path)  ALLOC_OBJ (path, PATH *, SIZEOF_PATH, PATH_LIST)

#define FREE_NODE(node)  FREE_OBJ (node, NODE_LIST)
#define FREE_TREE(tree)  FREE_OBJ (tree, TREE_LIST)
#define FREE_PATH(path)  FREE_OBJ (path, PATH_LIST)

/*===========================================================================*/

TREE *avl__tree (int treetype, uint keyoffs, int(*usrcmp)(void *,void *))
{
TREE *tree;
int   keyinfo;

keyinfo = treetype << 2;
switch (treetype)
  {
  case AVL_NODUP_MBR:case AVL_DUP_MBR: keyinfo |= USR_CMP;break;
  case AVL_NODUP_PTR:case AVL_DUP_PTR: keyinfo |= USR_CMP;break;
  case AVL_NODUP_STR:case AVL_DUP_STR: keyinfo |= STR_CMP;break;
  case AVL_NODUP_LNG:case AVL_DUP_LNG: keyinfo |= VAL_CMP;break;
  case AVL_NODUP_INT:case AVL_DUP_INT: keyinfo |= VAL_CMP;break;
  case AVL_NODUP_SHT:case AVL_DUP_SHT: keyinfo |= VAL_CMP;break;
  case AVL_NODUP_ULN:case AVL_DUP_ULN: keyinfo |= COR_CMP;break;
  case AVL_NODUP_UIN:case AVL_DUP_UIN: keyinfo |= COR_CMP;break;
  case AVL_NODUP_USH:case AVL_DUP_USH: keyinfo |= VAL_CMP;break;
  case AVL_NODUP_CHR:case AVL_DUP_CHR: keyinfo |= VAL_CMP;break;
  default:
    return NIL;
  }
 ALLOC_TREE (tree);
if (!tree)
  return NIL;
tree->root = NIL;
tree->keyinfo = (ushort)keyinfo;
tree->keyoffs = (ushort)keyoffs;
tree->usrcmp  = usrcmp;
tree->nodes   = 0L;
tree->path    = NIL;

return tree;
}

/*===========================================================================*/

static int rebalance (NODE **rootaddr)
{
NODE *root = *rootaddr;
NODE *newroot;
NODE *half;

switch (root->bal)
  {
  case LEFTUNBAL:
    switch (root->left->bal)
      {
      case LEFT: /* simple rotation, tree depth decreased */
        newroot = root->left;
        root->left = newroot->right;
        root->bal  = BAL;
        newroot->right = root;
        newroot->bal   = BAL;
        *rootaddr = newroot;
        return LESS;
	break;
	
      case BAL: /* simple rotation, tree depth unchanged */
        newroot = root->left;
        root->left = newroot->right;
        root->bal  = LEFT;
        newroot->right = root;
        newroot->bal   = RIGHT;
        *rootaddr = newroot;
        return SAME;
	break;

      case RIGHT: /* double rotation */
        half    = root->left;
        newroot = half->right;
        root->left  = newroot->right;
        half->right = newroot->left;
        switch (newroot->bal)
          {
          case BAL:
            root->bal = BAL;
            half->bal = BAL;
	    break;
          case LEFT:
            root->bal = RIGHT;
            half->bal = BAL;
	    break;
          case RIGHT:
            root->bal = BAL;
            half->bal = LEFT;
	    break;
          }
        newroot->left  = half;
        newroot->right = root;
        newroot->bal   = BAL;
        *rootaddr = newroot;
        return LESS;
	break;
      }
    break;
  case RIGHTUNBAL:
    switch (root->right->bal)
      {
      case RIGHT: /* simple rotation, tree depth decreased */
        newroot = root->right;
        root->right = newroot->left;
        root->bal   = BAL;
        newroot->left = root;
        newroot->bal  = BAL;
        *rootaddr = newroot;
        return LESS;
	break;

      case BAL: /* simple rotation, tree depth unchanged */
        newroot = root->right;
        root->right = newroot->left;
        root->bal   = RIGHT;
        newroot->left = root;
        newroot->bal  = LEFT;
        *rootaddr = newroot;
        return SAME;
	break;

      case LEFT: /* double rotation */
        half    = root->right;
        newroot = half->left;
        root->right = newroot->left;
        half->left  = newroot->right;
        switch (newroot->bal)
          {
          case BAL:
            root->bal = BAL;
            half->bal = BAL;
	    break;
          case RIGHT:
            root->bal = LEFT;
            half->bal = BAL;
	    break;
          case LEFT:
            root->bal = BAL;
            half->bal = RIGHT;
	    break;
          }
        newroot->right = half;
        newroot->left  = root;
        newroot->bal   = BAL;
        *rootaddr = newroot;
        return LESS;
	break;
      }
    break;
  default:
    return SAME;
  }
return ERROR;
}

/*===========================================================================*/

static int insert_ptr (NODE **rootaddr,NODE *node,int (*usrcmp)(void *,void *),int dup)
{
NODE *root = *rootaddr;
int cmp;
int ins;

SET_PTRCMP (cmp, usrcmp, node->key.ptr, root->key.ptr)
if (cmp < 0)
  {
  if (root->left)
    ins = insert_ptr (&root->left, node, usrcmp, dup);
  else
    {
    root->left = node;
    ins = DEEPER;
    }
  switch (ins)
    {
    case DEEPER:
      switch (root->bal)
        {
        case RIGHT:
          root->bal = BAL;
          return INS;
	  break;
        case BAL:
          root->bal = LEFT;
          return DEEPER;
	  break;
        case LEFT:
          root->bal = LEFTUNBAL;
          return rebalance (rootaddr) == LESS ? INS : DEEPER;
        }
      break;
    case INS:
      return INS;
      break;
    case NOTINS:
      return NOTINS;
    }
  }
else if (cmp > 0 || dup)
  {
  if (root->right)
    ins = insert_ptr (&root->right, node, usrcmp, dup);
  else
    {
    root->right = node;
    ins = DEEPER;
    }
  switch (ins)
    {
    case DEEPER:
      switch (root->bal)
        {
        case LEFT:
          root->bal = BAL;
          return INS;
	  break;
        case BAL:
          root->bal = RIGHT;
          return DEEPER;
	  break;
        case RIGHT:
          root->bal = RIGHTUNBAL;
          return rebalance (rootaddr) == LESS ? INS : DEEPER;
        }
      break;
    case INS:
      return INS;
      break;
    case NOTINS:
      return NOTINS;
    }
  }
return NOTINS;
}

/*---------------------------------------------------------------------------*/

static int insert_val (NODE **rootaddr, NODE *node,int dup)
{
NODE *root = *rootaddr;
int ins;

if (node->key.val < root->key.val)
  {
  if (root->left)
    ins = insert_val (&root->left, node, dup);
  else
    {
    root->left = node;
    ins = DEEPER;
    }
  switch (ins)
    {
    case DEEPER:
      switch (root->bal)
        {
        case RIGHT:
          root->bal = BAL;
          return INS;
	  break;
        case BAL:
          root->bal = LEFT;
          return DEEPER;
	  break;
        case LEFT:
          root->bal = LEFTUNBAL;
          return rebalance (rootaddr) == LESS ? INS : DEEPER;
	  break;
        }
      break;
    case INS:
      return INS;
      break;
    case NOTINS:
      return NOTINS;
      break;
    }
  }
else if (node->key.val > root->key.val || dup)
  {
  if (root->right)
    ins = insert_val (&root->right, node, dup);
  else
    {
    root->right = node;
    ins = DEEPER;
    }
  switch (ins)
    {
    case DEEPER:
      switch (root->bal)
        {
        case LEFT:
          root->bal = BAL;
          return INS;
	  break;
        case BAL:
          root->bal = RIGHT;
          return DEEPER;
	  break;
        case RIGHT:
          root->bal = RIGHTUNBAL;
          return rebalance (rootaddr) == LESS ? INS : DEEPER;
	  break;
        }
      break;
    case INS:
      return INS;
      break;
    case NOTINS:
      return NOTINS;
      break;
    }
  }
return NOTINS;
}

/*---------------------------------------------------------------------------*/

int avl_insert (TREE *tree,void *data)
{
NODE *node;
int   keyinfo;
int   ins;

 ALLOC_NODE (node);
if (!node)
  return FALSE;
node->data  = data;
node->left  = NIL;
node->right = NIL;
node->bal   = BAL;
keyinfo = tree->keyinfo;

switch (KEYTYPE (keyinfo))
  {
  case MBR_KEY: node->key.ptr =  (void   *)((char *)data + tree->keyoffs);
    break;
  case PTR_KEY: node->key.ptr = *(void  **)((char *)data + tree->keyoffs);
    break;
  case STR_KEY: node->key.ptr = *(void  **)((char *)data + tree->keyoffs);
    break;
  case LNG_KEY: node->key.val = *(long   *)((char *)data + tree->keyoffs);
    break;
  case INT_KEY: node->key.val = *(int    *)((char *)data + tree->keyoffs);
    break;
  case SHT_KEY: node->key.val = *(short  *)((char *)data + tree->keyoffs);
    break;
  case ULN_KEY: node->key.val =
                       CORRECT (*(ulong  *)((char *)data + tree->keyoffs));
    break;
  case UIN_KEY: node->key.val =
                       CORRECT (*(uint   *)((char *)data + tree->keyoffs));
    break;
  case USH_KEY: node->key.val = *(ushort *)((char *)data + tree->keyoffs);
    break;
  case CHR_KEY: node->key.val = *          ((char *)data + tree->keyoffs);
    break;
  default:
    FREE_NODE (node);
    return FALSE;
  }
if (tree->root)
  {
  switch (LOCTYPE (keyinfo))
    {
    case USR_NODUP: ins = insert_ptr (&tree->root, node, tree->usrcmp, NODUP);
      break;
    case STR_NODUP: ins = insert_ptr (&tree->root, node, AVL_AVLCMP, NODUP);
      break;
    case COR_NODUP:
    case VAL_NODUP: ins = insert_val (&tree->root, node, NODUP);
      break;
    case USR_DUP:   ins = insert_ptr (&tree->root, node, tree->usrcmp, DUP);
      break;
    case STR_DUP:   ins = insert_ptr (&tree->root, node, AVL_AVLCMP, DUP);
      break;
    case COR_DUP:
    case VAL_DUP:   ins = insert_val (&tree->root, node, DUP);
      break;
    }
  if (ins == NOTINS)
    {
      FREE_NODE (node);
    return FALSE;
    }
  }
else
  {
  tree->root = node;
  }
tree->nodes++;
if (tree->path)
  {
    FREE_PATH (tree->path);
  tree->path = NIL;
  }
return TRUE;
}

/*===========================================================================*/

#define SELECT_EQ_NODUP(node, val, this)           \
  {                                                \
  if      (this < val       ) node = node->right;  \
  else if (       val < this) node = node->left;   \
  else                        return node->data;   \
  }

#define SELECT_EQ_DUP(node, val, this, save)                      \
  {                                                               \
  if      (this < val       )                node = node->right;  \
  else if (       val < this)                node = node->left;   \
  else                        { save = node; node = node->left; } \
  }

#define SELECT_GE_NODUP(node, val, this, save)                    \
  {                                                               \
  if      (       val < this) { save = node; node = node->left; } \
  else if (this < val       )                node = node->right;  \
  else                                       return node->data;   \
  }

#define SELECT_GE_DUP(node, val, this, save)                      \
  {                                                               \
  if      (this < val       )                node = node->right;  \
  else                        { save = node; node = node->left; } \
  }

#define SELECT_GT(node, val, this, save)                          \
  {                                                               \
  if      (       val < this) { save = node; node = node->left; } \
  else                                       node = node->right;  \
  }

#define SELECT_LE_NODUP(node, val, this, save)                     \
  {                                                                \
  if      (this < val       ) { save = node; node = node->right; } \
  else if (       val < this)                node = node->left;    \
  else                                       return node->data;    \
  }

#define SELECT_LE_DUP(node, val, this, save)                       \
  {                                                                \
  if      (       val < this)                node = node->left;    \
  else                        { save = node; node = node->right; } \
  }

#define SELECT_LT(node, val, this, save)                           \
  {                                                                \
  if      (this < val       ) { save = node; node = node->right; } \
  else                                       node = node->left;    \
  }

/*---------------------------------------------------------------------------*/

void *avl__locate (TREE *tree, long keyval)
{
NODE *node;
NODE *save;
int (*usrcmp)(void *,void *);
int   cmp;

node = tree->root;
switch (LOCTYPE (tree->keyinfo))
  {
  case USR_NODUP:
    usrcmp = tree->usrcmp;
    while (node)
      {
      cmp = (*usrcmp) ((void *)keyval, node->key.ptr);
      SELECT_EQ_NODUP (node, cmp, 0)
      }
    break;
  case STR_NODUP:
    while (node)
      {
      SET_STRCMP (cmp, (char *)keyval, (char *)node->key.ptr)
      SELECT_EQ_NODUP (node, cmp, 0)
      }
    break;
  case COR_NODUP: keyval = CORRECT (keyval);
  case VAL_NODUP:
    while (node)
      SELECT_EQ_NODUP (node, keyval, node->key.val)
    break;
  case USR_DUP:
    save = NIL;
    usrcmp = tree->usrcmp;
    while (node)
      {
      cmp = (*usrcmp) ((void *)keyval, node->key.ptr);
      SELECT_EQ_DUP (node, cmp, 0, save)
      }
    if (save)
      return save->data;
    break;

  case STR_DUP:
    save = NIL;
    while (node)
      {
      SET_STRCMP (cmp, (char *)keyval, (char *)node->key.ptr)
      SELECT_EQ_DUP (node, cmp, 0, save)
      }
    if (save)
      return save->data;
    break;

  case COR_DUP: keyval = CORRECT (keyval);
  case VAL_DUP:
    save = NIL;
    while (node)
      SELECT_EQ_DUP (node, keyval, node->key.val, save)
    if (save)
      return save->data;
  }
return NIL;
}

/*---------------------------------------------------------------------------*/

void *avl__locate_ge (TREE *tree, long keyval)
{
NODE *node;
NODE *save;
int (*usrcmp)(void *,void *);
int   cmp;

node = tree->root;
save = NIL;
switch (LOCTYPE (tree->keyinfo))
  {
  case USR_NODUP:
    usrcmp = tree->usrcmp;
    while (node)
      {
      cmp = (*usrcmp) ((void *)keyval, node->key.ptr);
      SELECT_GE_NODUP (node, cmp, 0, save)
      }
    break;
  case STR_NODUP:
    while (node)
      {
      SET_STRCMP (cmp, (char *)keyval, (char *)node->key.ptr)
      SELECT_GE_NODUP (node, cmp, 0, save)
      }
    break;
  case COR_NODUP: keyval = CORRECT (keyval);
  case VAL_NODUP:
    while (node)
      SELECT_GE_NODUP (node, keyval, node->key.val, save)
    break;
  case USR_DUP:
    usrcmp = tree->usrcmp;
    while (node)
      {
      cmp = (*usrcmp) ((void *)keyval, node->key.ptr);
      SELECT_GE_DUP (node, cmp, 0, save)
      }
    break;
  case STR_DUP:
    while (node)
      {
      SET_STRCMP (cmp, (char *)keyval, (char *)node->key.ptr)
      SELECT_GE_DUP (node, cmp, 0, save)
      }
    break;
  case COR_DUP: keyval = CORRECT (keyval);
  case VAL_DUP:
    while (node)
      SELECT_GE_DUP (node, keyval, node->key.val, save)
  }
if (save)
  return save->data;
else
  return NIL;
}

/*---------------------------------------------------------------------------*/

void *avl__locate_gt (TREE *tree, long keyval)
{
NODE *node;
NODE *save;
int (*usrcmp)(void *,void *);
int   cmp;

node = tree->root;
save = NIL;
switch (CMPTYPE (tree->keyinfo))
  {
  case USR_CMP:
    usrcmp = tree->usrcmp;
    while (node)
      {
      cmp = (*usrcmp) ((void *)keyval, node->key.ptr);
      SELECT_GT (node, cmp, 0, save)
      }
    break;
  case STR_CMP:
    while (node)
      {
      SET_STRCMP (cmp, (char *)keyval, (char *)node->key.ptr)
      SELECT_GT (node, cmp, 0, save)
      }
    break;
  case COR_CMP: keyval = CORRECT (keyval);
  case VAL_CMP:
    while (node)
      SELECT_GT (node, keyval, node->key.val, save)
  }
if (save)
  return save->data;
else
  return NIL;
}

/*---------------------------------------------------------------------------*/

void *avl__locate_le (TREE *tree,long keyval)
{
NODE *node;
NODE *save;
int (*usrcmp)(void *, void *);
int   cmp;

node = tree->root;
save = NIL;
switch (LOCTYPE (tree->keyinfo))
  {
  case USR_NODUP:
    usrcmp = tree->usrcmp;
    while (node)
      {
      cmp = (*usrcmp) ((void *)keyval, node->key.ptr);
      SELECT_LE_NODUP (node, cmp, 0, save)
      }
    break;
  case STR_NODUP:
    while (node)
      {
      SET_STRCMP (cmp, (char *)keyval, (char *)node->key.ptr)
      SELECT_LE_NODUP (node, cmp, 0, save)
      }
    break;
  case COR_NODUP: keyval = CORRECT (keyval);
  case VAL_NODUP:
    while (node)
      SELECT_LE_NODUP (node, keyval, node->key.val, save)
    break;
  case USR_DUP:
    usrcmp = tree->usrcmp;
    while (node)
      {
      cmp = (*usrcmp) ((void *)keyval, node->key.ptr);
      SELECT_LE_DUP (node, cmp, 0, save)
      }
    break;
  case STR_DUP:
    while (node)
      {
      SET_STRCMP (cmp, (char *)keyval, (char *)node->key.ptr)
      SELECT_LE_DUP (node, cmp, 0, save)
      }
    break;
  case COR_DUP: keyval = CORRECT (keyval);
  case VAL_DUP:
    while (node)
      SELECT_LE_DUP (node, keyval, node->key.val, save)
  }
if (save)
  return save->data;
else
  return NIL;
}

/*---------------------------------------------------------------------------*/

void *avl__locate_lt (TREE *tree, long keyval)
{
NODE *node;
NODE *save;
int (*usrcmp)(void *,void *);
int   cmp;

node = tree->root;
save = NIL;
switch (CMPTYPE (tree->keyinfo))
  {
  case USR_CMP:
    usrcmp = tree->usrcmp;
    while (node)
      {
      cmp = (*usrcmp) ((void *)keyval, node->key.ptr);
      SELECT_LT (node, cmp, 0, save)
      }
    break;
  case STR_CMP:
    while (node)
      {
      SET_STRCMP (cmp, (char *)keyval, (char *)node->key.ptr)
      SELECT_LT (node, cmp, 0, save)
      }
    break;
  case COR_CMP: keyval = CORRECT (keyval);
  case VAL_CMP:
    while (node)
      SELECT_LT (node, keyval, node->key.val, save)
  }
if (save)
  return save->data;
else
  return NIL;
}

/*---------------------------------------------------------------------------*/

void *avl_locate_first (TREE *tree)
{
NODE *node, *leftnode;

node = tree->root;
if (!node)
  return NIL;
while ((leftnode = node->left))
  node = leftnode;
return node->data;
}

/*---------------------------------------------------------------------------*/

void *avl_locate_last (TREE *tree)
{
NODE *node, *rightnode;

node = tree->root;
if (!node)
  return NIL;
while ((rightnode = node->right))
  node = rightnode;
return node->data;
}

/*===========================================================================*/

static NODE *leftmost (NODE **rootaddr)
{
NODE *root = *rootaddr;
NODE *node;

if (root)
  {
  if (root->left)
    {
    node = leftmost (&root->left);
    if (node->bal == LESS)
      {
      /* left subtree depth decreased */
      switch (root->bal)
        {
        case LEFT:
          root->bal = BAL;
	  break;
        case BAL:
          root->bal = RIGHT;
          node->bal = SAME;
	  break;
        case RIGHT:
          root->bal = RIGHTUNBAL;
          node->bal = rebalance (rootaddr);
        }
      }
    return node;
    }
  else
    {
    *rootaddr = root->right;
    root->bal = LESS;
    return root;
    }
  }
else
  return NIL;
}

/*---------------------------------------------------------------------------*/

static NODE *remove_ptr (NODE **rootaddr, void *keyptr,int (*usrcmp)(void *,void *),int dup)
{
NODE *root = *rootaddr;
NODE *node;
int   cmp;

SET_PTRCMP (cmp, usrcmp, keyptr, root->key.ptr)
if (cmp < 0)
  {
  if (!root->left)
    return NIL;
  node = remove_ptr (&root->left, keyptr, usrcmp, dup);
  if (node && node->bal == LESS)
    {
    /* left subtree depth decreased */
    switch (root->bal)
      {
      case LEFT:
        root->bal = BAL;
	break;
      case BAL:
        root->bal = RIGHT;
        node->bal = SAME;
	break;
      case RIGHT:
        root->bal = RIGHTUNBAL;
        node->bal = rebalance (rootaddr);
      }
    }
  }
else if (cmp > 0)
  {
  if (!root->right)
    return NIL;
  node = remove_ptr (&root->right, keyptr, usrcmp, dup);
  if (node && node->bal == LESS)
    {
    /* right subtree depth decreased */
    switch (root->bal)
      {
      case RIGHT:
        root->bal = BAL;
	break;
      case BAL:
        root->bal = LEFT;
        node->bal = SAME;
	break;
      case LEFT:
        root->bal = LEFTUNBAL;
        node->bal = rebalance (rootaddr);
      }
    }
  }
else
  {
  if (dup && root->left
          && (node = remove_ptr (&root->left, keyptr, usrcmp, dup)))
    {
    if (node->bal == LESS)
      {
      /* left subtree depth decreased */
      switch (root->bal)
        {
        case LEFT:
          root->bal = BAL;
	  break;
        case BAL:
          root->bal = RIGHT;
          node->bal = SAME;
	  break;
        case RIGHT:
          root->bal = RIGHTUNBAL;
          node->bal = rebalance (rootaddr);
        }
      }
    }
  else
    {
    node = root;
    if (!node->right)
      {
      *rootaddr = node->left;
      node->bal = LESS;
      }
    else if (!node->left)
      {
      *rootaddr = node->right;
      node->bal = LESS;
      }
    else /* replace by the leftmost node of the right subtree */
      {
      root = leftmost (&node->right);
      root->left  = node->left;
      root->right = node->right;
      if (root->bal == LESS)
        {
        /* right subtree depth decreased */
        switch (node->bal)
          {
          case RIGHT:
            root->bal = BAL;
            node->bal = LESS;
	    break;
          case BAL:
            root->bal = LEFT;
            node->bal = SAME;
	    break;
          case LEFT:
            root->bal = LEFTUNBAL;
            node->bal = rebalance (&root);
          }
        }
      else
        {
        root->bal = node->bal;
        node->bal = SAME;
        }
      *rootaddr = root;
      }
    }
  }
return node;
}

/*---------------------------------------------------------------------------*/

static NODE *remove_val (NODE **rootaddr,long keyval,int dup)
{
NODE *root = *rootaddr;
NODE *node;

if (keyval < root->key.val)
  {
  if (!root->left)
    return NIL;
  node = remove_val (&root->left, keyval, dup);
  if (node && node->bal == LESS)
    {
    /* left subtree depth decreased */
    switch (root->bal)
      {
      case LEFT:
        root->bal = BAL;
	break;
      case BAL:
        root->bal = RIGHT;
        node->bal = SAME;
	break;
      case RIGHT:
        root->bal = RIGHTUNBAL;
        node->bal = rebalance (rootaddr);
      }
    }
  }
else if (keyval > root->key.val)
  {
  if (!root->right)
    return NIL;
  node = remove_val (&root->right, keyval, dup);
  if (node && node->bal == LESS)
    {
    /* right subtree depth decreased */
    switch (root->bal)
      {
      case RIGHT:
        root->bal = BAL;
	break;
      case BAL:
        root->bal = LEFT;
        node->bal = SAME;
	break;
      case LEFT:
        root->bal = LEFTUNBAL;
        node->bal = rebalance (rootaddr);
      }
    }
  }
else
  {
  if (dup && root->left
          && (node = remove_val (&root->left, keyval, dup)))
    {
    if (node->bal == LESS)
      {
      /* left subtree depth decreased */
      switch (root->bal)
        {
        case LEFT:
          root->bal = BAL;
	  break;
        case BAL:
          root->bal = RIGHT;
          node->bal = SAME;
	  break;
        case RIGHT:
          root->bal = RIGHTUNBAL;
          node->bal = rebalance (rootaddr);
        }
      }
    }
  else
    {
    node = root;
    if (!node->right)
      {
      *rootaddr = node->left;
      node->bal = LESS;
      }
    else if (!node->left)
      {
      *rootaddr = node->right;
      node->bal = LESS;
      }
    else /* replace by the leftmost node of the right subtree */
      {
      root = leftmost (&node->right);
      root->left  = node->left;
      root->right = node->right;
      if (root->bal == LESS)
        {
        /* right subtree depth decreased */
        switch (node->bal)
          {
          case RIGHT:
            root->bal = BAL;
            node->bal = LESS;
	    break;
          case BAL:
            root->bal = LEFT;
            node->bal = SAME;
	    break;
          case LEFT:
            root->bal = LEFTUNBAL;
            node->bal = rebalance (&root);
          }
        }
      else
        {
        root->bal = node->bal;
        node->bal = SAME;
        }
      *rootaddr = root;
      }
    }
  }
return node;
}

/*---------------------------------------------------------------------------*/

void *avl__remove (TREE *tree,long keyval)
{
NODE *node;
void *data;

if (tree->root)
  {
  switch (LOCTYPE (tree->keyinfo))
    {
    case USR_NODUP:
      node = remove_ptr (&tree->root, (void *)keyval, tree->usrcmp, NODUP);
      break;
    case STR_NODUP:
      node = remove_ptr (&tree->root, (void *)keyval, AVL_AVLCMP, NODUP);
      break;
    case COR_NODUP: keyval = CORRECT (keyval);
    case VAL_NODUP:
      node = remove_val (&tree->root, keyval, NODUP);
      break;
    case USR_DUP:
      node = remove_ptr (&tree->root, (void *)keyval, tree->usrcmp, DUP);
      break;
    case STR_DUP:
      node = remove_ptr (&tree->root, (void *)keyval, AVL_AVLCMP, DUP);
      break;
    case COR_DUP: keyval = CORRECT (keyval);
    case VAL_DUP:
      node = remove_val (&tree->root, keyval, DUP);
    }
  if (node)
    {
    tree->nodes--;
    if (tree->path)
      {
	FREE_PATH (tree->path);
      tree->path = NIL;
      }
    data = node->data;
    FREE_NODE (node);
    return data;
    }
  }
return NIL;
}

/*===========================================================================*/

static void scan_subtree (NODE *root, UsrFun usrfun)
{
if (root->left)  scan_subtree (root->left,  usrfun);
(*usrfun) (root->data,0);
if (root->right) scan_subtree (root->right, usrfun);
}

/*---------------------------------------------------------------------------*/

static void backscan_subtree (NODE *root,UsrFun usrfun)
{
if (root->right) backscan_subtree (root->right, usrfun);
(*usrfun) (root->data,0);
if (root->left)  backscan_subtree (root->left,  usrfun);
}

/*---------------------------------------------------------------------------*/

void avl__scan (TREE *tree, UsrFun usrfun, int back)
{
if (tree->root)
  {
  if (back)
    backscan_subtree (tree->root, usrfun);
  else
    scan_subtree (tree->root, usrfun);
  }
}

/*===========================================================================*/

void *avl_first (TREE *tree)
{
PATH *path=0;
char *pathright=0;
NODE **pathnode=0;
NODE *node=0;

if (!tree->root)
  return NIL;
if (!tree->path)
  {
    ALLOC_PATH(path);
  if (!path)
    return NIL;
  tree->path = path;
  }
else
  {
  path = tree->path;
  }
pathnode  = &(path->node[0]);
pathright = &(path->right[1]);
*  pathnode  = NIL; /* sentinels */
*  pathright = TRUE;
*++pathnode  = NIL;
*++pathright = FALSE;
*++pathnode  = node = tree->root;
while ((node = node->left))
  {
  *++pathright = FALSE;
  *++pathnode  = node;
  }
path->pathright = pathright;
path->pathnode  = pathnode;
return (*pathnode)->data;
}

/*---------------------------------------------------------------------------*/

void *avl_last (TREE *tree)
{
PATH *path;
char *pathright;
NODE **pathnode;
NODE *node;

if (!tree->root)
  return NIL;
if (!tree->path)
  {
    ALLOC_PATH (path);
  if (!path)
    return NIL;
  tree->path = path;
  }
else
  {
  path = tree->path;
  }
pathnode  = &path->node [0];
pathright = &path->right[1];
*  pathnode  = NIL; /* sentinels */
*  pathright = FALSE;
*++pathnode  = NIL;
*++pathright = TRUE;
*++pathnode  = node = tree->root;
while ((node = node->right))
  {
  *++pathright = TRUE;
  *++pathnode  = node;
  }
path->pathright = pathright;
path->pathnode  = pathnode;
return (*pathnode)->data;
}

/*---------------------------------------------------------------------------*/

#define DOWN_RIGHT_OR_BREAK(node, pathright, pathnode) \
  {                       \
  if ( (node = node->right) ) \
    {                     \
    *++pathright = TRUE;  \
    *++pathnode  = node;  \
    }                     \
  else                    \
    break;                \
  }

#define DOWN_LEFT_OR_BREAK(node, pathright, pathnode) \
  {                       \
  if ( (node = node->left) )  \
    {                     \
    *++pathright = FALSE; \
    *++pathnode  = node;  \
    }                     \
  else                    \
    break;                \
  }

#define START_OK_AND_RETURN(path, _pathright, _pathnode) \
  {                             \
  path->pathright = _pathright; \
  path->pathnode  = _pathnode;  \
  return (*_pathnode)->data;    \
  }

/*---------------------------------------------------------------------------*/

void *avl__start (TREE *tree, long keyval, int back)
{
PATH *path;
char *pathright;
NODE **pathnode;
NODE *node;
char *saveright;
NODE **savenode;
int (*usrcmp)(void *,void *);
int   cmp;

if (!tree->root)
  return NIL;
if (!tree->path)
  {
    ALLOC_PATH (path);
  if (!path)
    return NIL;
  tree->path = path;
  }
else
  {
  path = tree->path;
  }
pathnode  = &path->node [0];
pathright = &path->right[1];
saveright = NIL;
savenode  = NIL;
*  pathnode  = NIL; /* sentinels */
*  pathright = !back;
*++pathnode  = NIL;
*++pathright = back;
*++pathnode  = node = tree->root;
switch (LOCTYPE (tree->keyinfo))
  {
  case USR_NODUP:
  case STR_NODUP:
    usrcmp = tree->usrcmp;
    if (back)
      LOOP
        {
        SET_PTRCMP (cmp, usrcmp, (void *)keyval, node->key.ptr)
        if (cmp > 0)
          {
          saveright = pathright;
          savenode  = pathnode;
          DOWN_RIGHT_OR_BREAK (node, pathright, pathnode)
          }
        else if (cmp < 0)
          DOWN_LEFT_OR_BREAK  (node, pathright, pathnode)
        else
          START_OK_AND_RETURN (path, pathright, pathnode)
        }
    else
      LOOP
        {
        SET_PTRCMP (cmp, usrcmp, (void *)keyval, node->key.ptr)
        if (cmp < 0)
          {
          saveright = pathright;
          savenode  = pathnode;
          DOWN_LEFT_OR_BREAK  (node, pathright, pathnode)
          }
        else if (cmp > 0)
          DOWN_RIGHT_OR_BREAK (node, pathright, pathnode)
        else
          START_OK_AND_RETURN (path, pathright, pathnode)
        }
    break;
  case COR_NODUP: keyval = CORRECT (keyval);
  case VAL_NODUP:
    if (back)
      LOOP
        {
        if (keyval > node->key.val)
          {
          saveright = pathright;
          savenode  = pathnode;
          DOWN_RIGHT_OR_BREAK (node, pathright, pathnode)
          }
        else if (keyval < node->key.val)
          DOWN_LEFT_OR_BREAK  (node, pathright, pathnode)
        else
          START_OK_AND_RETURN (path, pathright, pathnode)
        }
    else
      LOOP
        {
        if (keyval < node->key.val)
          {
          saveright = pathright;
          savenode  = pathnode;
          DOWN_LEFT_OR_BREAK  (node, pathright, pathnode)
          }
        else if (keyval > node->key.val)
          DOWN_RIGHT_OR_BREAK (node, pathright, pathnode)
        else
          START_OK_AND_RETURN (path, pathright, pathnode)
        }
    break;
  case USR_DUP:
  case STR_DUP:
    usrcmp = tree->usrcmp;
    if (back)
      LOOP
        {
        SET_PTRCMP (cmp, usrcmp, (void *)keyval, node->key.ptr)
        if (cmp >= 0)
          {
          saveright = pathright;
          savenode  = pathnode;
          DOWN_RIGHT_OR_BREAK (node, pathright, pathnode)
          }
        else
          DOWN_LEFT_OR_BREAK  (node, pathright, pathnode)
        }
    else
      LOOP
        {
        SET_PTRCMP (cmp, usrcmp, (void *)keyval, node->key.ptr)
        if (cmp <= 0)
          {
          saveright = pathright;
          savenode  = pathnode;
          DOWN_LEFT_OR_BREAK  (node, pathright, pathnode)
          }
        else
          DOWN_RIGHT_OR_BREAK (node, pathright, pathnode)
        }
    break;
  case COR_DUP: keyval = CORRECT (keyval);
  case VAL_DUP:
    if (back)
      LOOP
        {
        if (keyval >= node->key.val)
          {
          saveright = pathright;
          savenode  = pathnode;
          DOWN_RIGHT_OR_BREAK (node, pathright, pathnode)
          }
        else
          DOWN_LEFT_OR_BREAK  (node, pathright, pathnode)
        }
    else
      LOOP
        {
        if (keyval <= node->key.val)
          {
          saveright = pathright;
          savenode  = pathnode;
          DOWN_LEFT_OR_BREAK  (node, pathright, pathnode)
          }
        else
          DOWN_RIGHT_OR_BREAK (node, pathright, pathnode)
        }
  }
if (savenode)
  {
  path->pathright = saveright;
  path->pathnode  = savenode;
  return (*savenode)->data;
  }
else
  {
    FREE_PATH (path);
  tree->path = NIL;
  return NIL;
  }
}

/*---------------------------------------------------------------------------*/

void *avl_next (TREE *tree)
{
PATH *path;
char *pathright;
NODE **pathnode;
NODE *node;

path = tree->path;
if (!path)
  return NIL;
pathright = path->pathright;
pathnode  = path->pathnode;
if ((node = (*pathnode)->right))
  {
  *++pathright = TRUE;
  *++pathnode  = node;
  while ((node = node->left))
    {
    *++pathright = FALSE;
    *++pathnode  = node;
    }
  }
else
  {
  while (*pathright)
    {
    --pathright;
    --pathnode;
    }
  --pathright;
  --pathnode;
  if (!*pathnode)
    {
      FREE_PATH (path);
    tree->path = NIL;
    return NIL;
    }
  }
path->pathright = pathright;
path->pathnode  = pathnode;
return (*pathnode)->data;
}

/*---------------------------------------------------------------------------*/

void *avl_prev (TREE *tree)
{
PATH *path;
char *pathright;
NODE **pathnode;
NODE *node;

path = tree->path;
if (!path)
  return NIL;
pathright = path->pathright;
pathnode  = path->pathnode;
if ((node = (*pathnode)->left))
  {
  *++pathright = FALSE;
  *++pathnode  = node;
  while ((node = node->right))
    {
    *++pathright = TRUE;
    *++pathnode  = node;
    }
  }
else
  {
  while (!*pathright)
    {
    --pathright;
    --pathnode;
    }
  --pathright;
  --pathnode;
  if (!*pathnode)
    {
      FREE_PATH (path);
    tree->path = NIL;
    return NIL;
    }
  }
path->pathright = pathright;
path->pathnode  = pathnode;
return (*pathnode)->data;
}

/*---------------------------------------------------------------------------*/

void  avl_stop (TREE *tree)
{
if (tree->path)
  {
    FREE_PATH (tree->path);
  tree->path = NIL;
  }
}

/*===========================================================================*/

static uint Offset;
static void *Save;

/*---------------------------------------------------------------------------*/

static void link_subtree (NODE *node)
{
if (node->right) link_subtree (node->right);
*(void **)((char *)node->data + Offset) = Save;
Save = node->data;
if (node->left)  link_subtree (node->left);
}

/*---------------------------------------------------------------------------*/

static void backlink_subtree (NODE *node)
{
if (node->left)  backlink_subtree (node->left);
*(void **)((char *)node->data + Offset) = Save;
Save = node->data;
if (node->right) backlink_subtree (node->right);
}

/*---------------------------------------------------------------------------*/

void *avl__link (TREE *tree, uint ptroffs, int back)
{
Offset = ptroffs;
Save = NIL;
if (tree->root)
  {
  if (back)
    backlink_subtree (tree->root);
  else
    link_subtree (tree->root);
  }
return Save;
}

/*===========================================================================*/

static void release_subtree (NODE *root, UsrFun usrfun)
{
if (root->left)  release_subtree (root->left,  usrfun);
if (root->right) release_subtree (root->right, usrfun);
(*usrfun) (root->data,0);
 FREE_NODE (root);
}

/*---------------------------------------------------------------------------*/

static void reset_subtree (NODE *root)
{
if (root->left)  reset_subtree (root->left);
if (root->right) reset_subtree (root->right);
 FREE_NODE (root);
}

/*---------------------------------------------------------------------------*/

void avl_release (TREE *tree, UsrFun usrfun)
{
if (tree->root)
  release_subtree (tree->root, usrfun);
tree->root  = NIL;
tree->nodes = 0;
if (tree->path)
  {
    FREE_PATH (tree->path);
  tree->path = NIL;
  }
}

/*---------------------------------------------------------------------------*/

void avl_reset (TREE *tree)
{
if (tree->root)
  reset_subtree (tree->root);
tree->root  = NIL;
tree->nodes = 0;
if (tree->path)
  {
    FREE_PATH (tree->path);
  tree->path = NIL;
  }
}

/*---------------------------------------------------------------------------*/

void avl_close (TREE *tree)
{
if (tree->root)
  reset_subtree (tree->root);
if (tree->path)
  FREE_PATH (tree->path);
tree->keyinfo = (ushort)ERROR;
 FREE_TREE (tree);
}

/*===========================================================================*/

static int copy_subtree (NODE *newroot, NODE *root)
{
newroot->key  = root->key;
newroot->data = root->data;
newroot->bal  = root->bal;

if (root->left)
  {
    ALLOC_NODE (newroot->left);
  if (!newroot->left)
    return FALSE;
  if (!copy_subtree (newroot->left,  root->left))
    {
      FREE_NODE (newroot->left);
    return FALSE;
    }
  }
else
  newroot->left = NIL;

if (root->right)
  {
    ALLOC_NODE (newroot->right);
  if (!newroot->right)
    return FALSE;
  if (!copy_subtree (newroot->right, root->right))
    {
      FREE_NODE (newroot->right);
    return FALSE;
    }
  }
else
  newroot->right = NIL;

return TRUE;
}

/*---------------------------------------------------------------------------*/

TREE *avl_copy (TREE *tree)
{
TREE *newtree;

ALLOC_TREE (newtree);
if (!newtree)
  return NIL;
newtree->keyinfo = tree->keyinfo;
newtree->keyoffs = tree->keyoffs;
newtree->usrcmp  = tree->usrcmp;
newtree->nodes   = tree->nodes;
newtree->path    = NIL;

if (tree->root)
  {
  ALLOC_NODE (newtree->root);
  if (!copy_subtree (newtree->root, tree->root))
    {
    FREE_NODE (newtree->root);
    avl_close (newtree);
    return NIL;
    }
  }
else
  {
  newtree->root = NIL;
  }
return newtree;
}

/*===========================================================================*/

static void **Dat;
static int   *Lev;
static int   *Pos;
static int    Nod;
static int    Max_Lev;

/*---------------------------------------------------------------------------*/

static void dump_subtree (struct avl_node *root, int lev, int pos)
{
if (root->left)  dump_subtree (root->left,  lev + 1, pos * 2    );
Dat[Nod] = root->data;
Lev[Nod] = lev;
Pos[Nod] = pos;
if (lev > Max_Lev)
  Max_Lev = lev;
Nod++;
if (root->right) dump_subtree (root->right, lev + 1, pos * 2 + 1);
}

/*---------------------------------------------------------------------------*/

int avl_dump (TREE *tree, void **dat_vect, int *lev_vect, int *pos_vect)
{
Dat = dat_vect;
Lev = lev_vect;
Pos = pos_vect;
Nod = 0;
Max_Lev = -1;
if (tree->root)
  dump_subtree (tree->root, 0, 0);
return Max_Lev + 1;
}

/*===========================================================================*/

int avl_porting_problems ()
{
long   lng1, lng2;
ushort ush1, ush2;
char   chr1, chr2;
void  *ptr1, *ptr2;
struct s { char c[4]; } *ps;
int    problems;

#define PROBLEM(n) (problems |= 1 | (1 << (n)))

problems = 0;

Avl_Dummy[0] = 0.0;
Avl_Dummy[1] = 0.0;
((char *)Avl_Dummy)[1] = 0x15;
ptr1 = (void *)((char *)Avl_Dummy + 1);
lng1 = (long)ptr1;
ptr2 = (void *)lng1;
if (*(char *)ptr2 != (char)0x15)
  PROBLEM (1);

ps = (struct s *)malloc (sizeof (struct s));
ps->c[0] = 0;
ps->c[1] = 0x15;
ps->c[2] = 0;
ps->c[3] = 0;
ptr1 = (void *)&ps->c[1];
lng1 = (long)ptr1;
ptr2 = (void *)lng1;
if (*(char *)ptr2 != (char)0x15)
  PROBLEM (2);
free (ps);

chr1 = (char)1;
chr2 = (char)250;
lng1 = chr1;
lng2 = chr2;
if ((chr1 > chr2) && (lng1 < lng2) || (chr1 < chr2) && (lng1 > lng2))
  PROBLEM (3);

if (sizeof (long) == sizeof (short))
  PROBLEM (4);

if (sizeof (char) != 1)
  PROBLEM (5);

ush1 = (ushort)-1;
ush2 = (ushort)-2;
lng1 = CORRECT (ush1);
lng2 = CORRECT (ush2);
if (!(lng1 > lng2))
  PROBLEM (6);

lng1 = CORRECT (0L);
lng2 = lng1 - 1;
if (!((lng1 < 0) && (lng2 > 0)))
  PROBLEM (7);

return problems;
}





























