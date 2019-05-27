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

#include "frog/tagger_base.h"

#include <algorithm>
#include "frog/Frog-util.h"

using namespace std;
using namespace Tagger;
using namespace icu;

#define LOG *TiCC::Log(err_log)
#define DBG *TiCC::Log(dbg_log)

BaseTagger::BaseTagger( TiCC::LogStream *errlog,
			TiCC::LogStream *dbglog,
			const string& label ){
  debug = 0;
  tagger = 0;
  filter = 0;
  _label = label;
  err_log = new TiCC::LogStream( errlog, _label + "-tagger-" );
  if ( dbglog ){
    dbg_log = new TiCC::LogStream( dbglog, _label + "-tagger-" );
  }
  else {
    dbg_log = err_log;
  }
}

BaseTagger::~BaseTagger(){
  delete tagger;
  delete filter;
  if ( err_log != dbg_log ){
    delete dbg_log;
  }
  delete err_log;
}

bool BaseTagger::fill_map( const string& file, map<string,string>& mp ){
  ifstream is( file );
  if ( !is ){
    return false;
  }
  string line;
  while( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    vector<string> parts;
    size_t num = TiCC::split_at( line, parts, "\t" );
    if ( num != 2 ){
      LOG << "invalid entry in '" << file << "'" << endl;
      LOG << "expected 2 tab-separated values, but got: '"
	  << line << "'" << endl;
      return false;
    }
    mp[ parts[0] ] = parts[1];
  }
  return true;
}

bool BaseTagger::init( const TiCC::Configuration& config ){
  if ( tagger != 0 ){
    LOG << _label << "-tagger is already initialized!" << endl;
    return false;
  }
  string val = config.lookUp( "debug", _label );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debug = TiCC::stringTo<int>( val );
  }
  switch ( debug ){
  case 0:
  case 1:
    dbg_log->setlevel(LogSilent);
    break;
  case 2:
    dbg_log->setlevel(LogNormal);
    break;
  case 3:
  case 4:
    dbg_log->setlevel(LogDebug);
    break;
  case 5:
  case 6:
  case 7:
    dbg_log->setlevel(LogHeavy);
    break;
  default:
    dbg_log->setlevel(LogExtreme);
  }
  val = config.lookUp( "settings", _label );
  if ( val.empty() ){
    LOG << "Unable to find settings for: " << _label << endl;
    return false;
  }
  string settings;
  if ( val[0] == '/' ) { // an absolute path
    settings = val;
  }
  else {
    settings =  config.configDir() + val;
  }
  val = config.lookUp( "version", _label );
  if ( val.empty() ){
    _version = "1.0";
  }
  else {
    _version = val;
  }
  val = config.lookUp( "set", _label );
  if ( val.empty() ){
    LOG << "missing 'set' declaration in config" << endl;
    return false;
  }
  else {
    tagset = val;
  }
  string charFile = config.lookUp( "char_filter_file", _label );
  if ( charFile.empty() ){
    charFile = config.lookUp( "char_filter_file" );
  }
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new TiCC::UniFilter();
    filter->fill( charFile );
  }
  string tokFile = config.lookUp( "token_trans_file", _label );
  if ( tokFile.empty() ){
    tokFile = config.lookUp( "token_trans_file" );
  }
  if ( !tokFile.empty() ){
    tokFile = prefix( config.configDir(), tokFile );
    if ( !fill_map( tokFile, token_tag_map ) ){
      LOG << "failed to load a token translation file from: '"
		   << tokFile << "'"<< endl;
      return false;
    }
  }
  string cls = config.lookUp( "outputclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }
  if ( debug > 1 ){
    DBG << _label << "-tagger textclass= " << textclass << endl;
  }
  string init = "-s " + settings + " -vcf";
  tagger = new MbtAPI( init, *dbg_log );
  return tagger->isInit();
}

void BaseTagger::add_provenance( folia::Document& doc,
				 folia::processor *main ) const {
  if ( !main ){
    throw logic_error( _label + "::add_provenance() without parent proc." );
  }
  folia::KWargs args;
  args["name"] = _label;
  //  args["id"] = _label + "." + folia::randnum(8) + ".1";
  args["id"] = _label + ".1";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  folia::processor *proc = doc.add_processor( args, main );
  add_declaration( doc, proc );
}


vector<TagResult> BaseTagger::tagLine( const string& line ){
  if ( tagger ){
    return tagger->TagLine(line);
  }
  throw runtime_error( _label + "-tagger is not initialized" );
}

string BaseTagger::set_eos_mark( const std::string& eos ){
  if ( tagger ){
    return tagger->set_eos_mark( eos );
  }
  throw runtime_error( _label + "-tagger is not initialized" );
}

string BaseTagger::extract_sentence( const vector<folia::Word*>& swords,
				     vector<string>& words ){
  words.clear();
  string sentence;
  for ( const auto& sword : swords ){
    UnicodeString word;
#pragma omp critical (foliaupdate)
    {
      word = sword->text( textclass );
    }
    if ( filter ){
      word = filter->filter( word );
    }
    string word_s = TiCC::UnicodeToUTF8( word );
    // the word may contain spaces, remove them all!
    word_s.erase(remove_if(word_s.begin(), word_s.end(), ::isspace), word_s.end());
    sentence += word_s;
    if ( &sword != &swords.back() ){
      sentence += " ";
    }
  }
  return sentence;
}

void BaseTagger::extract_words_tags(  const vector<folia::Word *>& swords,
				      const string& tagset,
				      vector<string>& words,
				      vector<string>& ptags ){
  for ( size_t i=0; i < swords.size(); ++i ){
    folia::Word *sw = swords[i];
    folia::PosAnnotation *postag = 0;
    UnicodeString word;
#pragma omp critical (foliaupdate)
    {
      word = sw->text( textclass );
      postag = sw->annotation<folia::PosAnnotation>( tagset );
    }
    if ( filter ){
      word = filter->filter( word );
    }
    // the word may contain spaces, remove them all!
    string word_s = TiCC::UnicodeToUTF8( word );
    word_s.erase(remove_if(word_s.begin(), word_s.end(), ::isspace), word_s.end());
    words.push_back( word_s );
    ptags.push_back( postag->cls() );
  }
}

string BaseTagger::extract_sentence( const frog_data& sent,
				     vector<string>& words ){
  words.clear();
  string sentence;
  for ( const auto& sword : sent.units ){
    icu::UnicodeString word = TiCC::UnicodeFromUTF8(sword.word);
    if ( filter ){
      word = filter->filter( word );
    }
    string word_s = TiCC::UnicodeToUTF8( word );
    // the word may contain spaces, remove them all!
    word_s.erase(remove_if(word_s.begin(), word_s.end(), ::isspace), word_s.end());
    sentence += word_s;
    sentence += " ";
  }
  return sentence;
}

void BaseTagger::Classify( frog_data& sent ){
  string sentence = extract_sentence( sent, _words );
  if ( debug > 1 ){
    DBG << _label << "-tagger in: " << sentence << endl;
  }
  _tag_result = tagger->TagLine(sentence);
  if ( _tag_result.size() != sent.size() ){
    LOG << _label << "-tagger mismatch between number of words and the tagger result." << endl;
    LOG << "words according to sentence: " << endl;
    for ( size_t w = 0; w < sent.size(); ++w ) {
      LOG << "w[" << w << "]= " << sent.units[w].word << endl;
    }
    LOG << "words according to " << _label << "-tagger: " << endl;
    for ( size_t i=0; i < _tag_result.size(); ++i ){
      LOG << "word[" << i << "]=" << _tag_result[i].word() << endl;
    }
    throw runtime_error( _label + "-tagger is confused" );
  }
  if ( debug > 1 ){
    DBG << _label + "-tagger out: " << endl;
    for ( size_t i=0; i < _tag_result.size(); ++i ){
      DBG << "[" << i << "] : word=" << _tag_result[i].word()
	  << " tag=" << _tag_result[i].assignedTag()
	  << " confidence=" << _tag_result[i].confidence() << endl;
    }
  }
  post_process( sent );
}
