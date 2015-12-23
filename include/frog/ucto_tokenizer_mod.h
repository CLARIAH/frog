/*
  Copyright (c) 2006 - 2016
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of frog.

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
      https://github.com/LanguageMachines/timblserver/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl

*/

#ifndef UCTO_TOKENIZER_MOD_H
#define UCTO_TOKENIZER_MOD_H

#include "ucto/tokenize.h"

class UctoTokenizer {
 public:
  UctoTokenizer(TiCC::LogStream *);
  ~UctoTokenizer() { delete tokenizer; delete uctoLog; };
  bool init( const TiCC::Configuration& );
  void setUttMarker( const std::string& );
  void setPassThru( bool );
  bool getPassThru() const;
  void setSentencePerLineInput( bool );
  void setInputEncoding( const std::string& );
  void setQuoteDetection( bool );
  void setInputXml( bool );
  void setTextClass( const std::string& );
  void setDocID( const std::string& );
  folia::Document tokenizestring( const std::string& );
  folia::Document tokenize( std::istream& );
  bool tokenize( folia::Document& );
  std::vector<std::string> tokenize( const std::string&  );
  std::string tokenizeStream( std::istream& );
 private:
  Tokenizer::TokenizerClass *tokenizer;
  TiCC::LogStream *uctoLog;
};

#endif
