/*
  Copyright (c) 2006 - 2015
  Tilburg University

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
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
*/

#ifndef IOB_TAGGER_MOD_H
#define IOB_TAGGER_MOD_H

class IOBTagger {
 public:
  IOBTagger(TiCC::LogStream *);
  ~IOBTagger();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& ) const;
  void Classify( const std::vector<folia::Word *>& );
  std::string getTagset() const { return tagset; };
  std::string set_eos_mark( const std::string& );
 private:
  void addChunk( folia::ChunkingLayer *,
		 const std::vector<folia::Word*>&,
		 const std::vector<double>&,
		 const std::string& );
  void addIOBTags( const std::vector<folia::Word*>&,
		   const std::vector<std::string>&,
		   const std::vector<double>& );
  MbtAPI *tagger;
  TiCC::LogStream *iobLog;
  int debug;
  std::string version;
  std::string tagset;
};

#endif // IOB_TAGGER_MOD_H
