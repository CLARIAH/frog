/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2019
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of frog:

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for
  several languages

  frog is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  frog is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/frog/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl

*/

#ifndef ALPINO_PARSER_H
#define ALPINO_PARSER_H

#include <string>
#include <vector>
#include <set>
#include <libxml/tree.h>

class frog_data;

struct dp_tree {
  int id;
  int begin;
  int end;
  int word_index;
  std::string word;
  std::string rel;
  dp_tree *link;
  dp_tree *next;
  ~dp_tree(){ delete link; delete next; };
};

std::ostream& operator<<( std::ostream& os, const dp_tree *node );

xmlDoc *alpino_server_parse( frog_data& fd );

void print_nodes( int indent, const dp_tree *store );

std::vector<std::pair<std::string,int>> extract_dp( xmlDoc * );

#endif
