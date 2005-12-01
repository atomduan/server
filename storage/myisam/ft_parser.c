/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

#include "ftdefs.h"

typedef struct st_ft_docstat {
  FT_WORD *list;
  uint uniq;
  double sum;
} FT_DOCSTAT;


typedef struct st_my_ft_parser_param
{
  TREE *wtree;
  my_bool with_alloc;
} MY_FT_PARSER_PARAM;


static int FT_WORD_cmp(CHARSET_INFO* cs, FT_WORD *w1, FT_WORD *w2)
{
  return mi_compare_text(cs, (uchar*) w1->pos, w1->len,
                         (uchar*) w2->pos, w2->len, 0, 0);
}

static int walk_and_copy(FT_WORD *word,uint32 count,FT_DOCSTAT *docstat)
{
    word->weight=LWS_IN_USE;
    docstat->sum+=word->weight;
    memcpy_fixed((docstat->list)++,word,sizeof(FT_WORD));
    return 0;
}

/* transforms tree of words into the array, applying normalization */

FT_WORD * ft_linearize(TREE *wtree)
{
  FT_WORD *wlist,*p;
  FT_DOCSTAT docstat;
  DBUG_ENTER("ft_linearize");

  if ((wlist=(FT_WORD *) my_malloc(sizeof(FT_WORD)*
				   (1+wtree->elements_in_tree),MYF(0))))
  {
    docstat.list=wlist;
    docstat.uniq=wtree->elements_in_tree;
    docstat.sum=0;
    tree_walk(wtree,(tree_walk_action)&walk_and_copy,&docstat,left_root_right);
  }
  delete_tree(wtree);
  if (!wlist)
    DBUG_RETURN(NULL);

  docstat.list->pos=NULL;

  for (p=wlist;p->pos;p++)
  {
    p->weight=PRENORM_IN_USE;
  }

  for (p=wlist;p->pos;p++)
  {
    p->weight/=NORM_IN_USE;
  }

  DBUG_RETURN(wlist);
}

my_bool ft_boolean_check_syntax_string(const byte *str)
{
  uint i, j;

  if (!str ||
      (strlen(str)+1 != sizeof(ft_boolean_syntax)) ||
      (str[0] != ' ' && str[1] != ' '))
    return 1;
  for (i=0; i<sizeof(ft_boolean_syntax); i++)
  {
    /* limiting to 7-bit ascii only */
    if ((unsigned char)(str[i]) > 127 || my_isalnum(default_charset_info, str[i]))
      return 1;
    for (j=0; j<i; j++)
      if (str[i] == str[j] && (i != 11 || j != 10))
        return 1;
  }
  return 0;
}

/*
  RETURN VALUE
  0 - eof
  1 - word found
  2 - left bracket
  3 - right bracket
  4 - stopword found
*/
byte ft_get_word(CHARSET_INFO *cs, byte **start, byte *end,
                 FT_WORD *word, MYSQL_FTPARSER_BOOLEAN_INFO *param)
{
  byte *doc=*start;
  uint mwc, length, mbl;

  param->yesno=(FTB_YES==' ') ? 1 : (param->quot != 0);
  param->weight_adjust= param->wasign= 0;
  param->type= FT_TOKEN_EOF;

  while (doc<end)
  {
    for (;doc<end;doc++)
    {
      if (true_word_char(cs,*doc)) break;
      if (*doc == FTB_RQUOT && param->quot)
      {
        param->quot=doc;
        *start=doc+1;
        param->type= FT_TOKEN_RIGHT_PAREN;
        goto ret;
      }
      if (!param->quot)
      {
        if (*doc == FTB_LBR || *doc == FTB_RBR || *doc == FTB_LQUOT)
        {
          /* param->prev=' '; */
          *start=doc+1;
          if (*doc == FTB_LQUOT) param->quot=*start;
          param->type= (*doc == FTB_RBR ? FT_TOKEN_RIGHT_PAREN : FT_TOKEN_LEFT_PAREN);
          goto ret;
        }
        if (param->prev == ' ')
        {
          if (*doc == FTB_YES ) { param->yesno=+1;    continue; } else
          if (*doc == FTB_EGAL) { param->yesno= 0;    continue; } else
          if (*doc == FTB_NO  ) { param->yesno=-1;    continue; } else
          if (*doc == FTB_INC ) { param->weight_adjust++; continue; } else
          if (*doc == FTB_DEC ) { param->weight_adjust--; continue; } else
          if (*doc == FTB_NEG ) { param->wasign= !param->wasign; continue; }
        }
      }
      param->prev=*doc;
      param->yesno=(FTB_YES==' ') ? 1 : (param->quot != 0);
      param->weight_adjust= param->wasign= 0;
    }

    mwc=length=0;
    for (word->pos=doc; doc<end; length++, mbl=my_mbcharlen(cs, *(uchar *)doc), doc+=(mbl ? mbl : 1))
      if (true_word_char(cs,*doc))
        mwc=0;
      else if (!misc_word_char(*doc) || mwc)
        break;
      else
        mwc++;

    param->prev='A'; /* be sure *prev is true_word_char */
    word->len= (uint)(doc-word->pos) - mwc;
    if ((param->trunc=(doc<end && *doc == FTB_TRUNC)))
      doc++;

    if (((length >= ft_min_word_len && !is_stopword(word->pos, word->len))
         || param->trunc) && length < ft_max_word_len)
    {
      *start=doc;
      param->type= FT_TOKEN_WORD;
      goto ret;
    }
    else if (length) /* make sure length > 0 (if start contains spaces only) */
    {
      *start= doc;
      param->type= FT_TOKEN_STOPWORD;
      goto ret;
    }
  }
  if (param->quot)
  {
    param->quot=*start=doc;
    param->type= 3; /* FT_RBR */
    goto ret;
  }
ret:
  return param->type;
}

byte ft_simple_get_word(CHARSET_INFO *cs, byte **start, const byte *end,
                        FT_WORD *word, my_bool skip_stopwords)
{
  byte *doc= *start;
  uint mwc, length, mbl;
  DBUG_ENTER("ft_simple_get_word");

  do
  {
    for (;; doc++)
    {
      if (doc >= end) DBUG_RETURN(0);
      if (true_word_char(cs, *doc)) break;
    }

    mwc= length= 0;
    for (word->pos=doc; doc<end; length++, mbl=my_mbcharlen(cs, *(uchar *)doc), doc+=(mbl ? mbl : 1))
      if (true_word_char(cs,*doc))
        mwc= 0;
      else if (!misc_word_char(*doc) || mwc)
        break;
      else
        mwc++;

    word->len= (uint)(doc-word->pos) - mwc;

    if (skip_stopwords == FALSE ||
        (length >= ft_min_word_len && length < ft_max_word_len &&
         !is_stopword(word->pos, word->len)))
    {
      *start= doc;
      DBUG_RETURN(1);
    }
  } while (doc < end);
  DBUG_RETURN(0);
}

void ft_parse_init(TREE *wtree, CHARSET_INFO *cs)
{
  DBUG_ENTER("ft_parse_init");
  if (!is_tree_inited(wtree))
    init_tree(wtree,0,0,sizeof(FT_WORD),(qsort_cmp2)&FT_WORD_cmp,0,NULL, cs);
  DBUG_VOID_RETURN;
}


static int ft_add_word(void *param, byte *word, uint word_len,
             MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info __attribute__((unused)))
{
  TREE *wtree;
  FT_WORD w;
  DBUG_ENTER("ft_add_word");
  wtree= ((MY_FT_PARSER_PARAM *)param)->wtree;
  if (((MY_FT_PARSER_PARAM *)param)->with_alloc)
  {
    byte *ptr;
    /* allocating the data in the tree - to avoid mallocs and frees */
    DBUG_ASSERT(wtree->with_delete == 0);
    ptr= (byte *)alloc_root(&wtree->mem_root, word_len);
    memcpy(ptr, word, word_len);
    w.pos= ptr;
  }
  else
    w.pos= word;
  w.len= word_len;
  if (!tree_insert(wtree, &w, 0, wtree->custom_arg))
  {
    delete_tree(wtree);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


static int ft_parse_internal(void *param, byte *doc, uint doc_len)
{
  byte   *end=doc+doc_len;
  FT_WORD w;
  TREE *wtree;
  DBUG_ENTER("ft_parse_internal");

  wtree= ((MY_FT_PARSER_PARAM *)param)->wtree;
  while (ft_simple_get_word(wtree->custom_arg, &doc, end, &w, TRUE))
    if (ft_add_word(param, w.pos, w.len, 0))
      DBUG_RETURN(1);
  DBUG_RETURN(0);
}


int ft_parse(TREE *wtree, byte *doc, int doclen, my_bool with_alloc,
                    struct st_mysql_ftparser *parser)
{
  MYSQL_FTPARSER_PARAM param;
  MY_FT_PARSER_PARAM my_param;
  DBUG_ENTER("ft_parse");
  DBUG_ASSERT(parser);
  my_param.wtree= wtree;
  my_param.with_alloc= with_alloc;

  param.mysql_parse= ft_parse_internal;
  param.mysql_add_word= ft_add_word;
  param.ftparser_state= 0;
  param.mysql_ftparam= &my_param;
  param.cs= wtree->custom_arg;
  param.doc= doc;
  param.length= doclen;
  param.mode= MYSQL_FTPARSER_SIMPLE_MODE;
  DBUG_RETURN(parser->parse(&param));
}
