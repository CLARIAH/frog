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
#include "ticcutils/SocketBasics.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/json.hpp"
#include "mbtserver/MbtServerBase.h"
#include "frog/Frog-util.h"

using namespace std;
using namespace Tagger;
using namespace icu;
using namespace nlohmann;

using TiCC::operator<<;

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
  string val = config.lookUp( "host", _label );
  if ( !val.empty() ){
    // assume we must use a MBT server for tagging
    host = val;
    val = config.lookUp( "port", _label );
    if ( val.empty() ){
      LOG << "missing 'port' settings for host= " << host << endl;
      return false;
    }
    port = val;
    val = config.lookUp( "base", _label );
    if ( !val.empty() ){
      base = val;
    }
  }
  else {
    val = config.lookUp( "port", _label );
    if ( !val.empty() ){
      LOG << "missing 'host' settings for port= " << port << endl;
      return false;
    }
  }
  val = config.lookUp( "debug", _label );
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
  string settings;
  if ( host.empty() ){
    val = config.lookUp( "settings", _label );
    if ( val.empty() ){
      LOG << "Unable to find settings for: " << _label << endl;
      return false;
    }
    if ( val[0] == '/' ) { // an absolute path
      settings = val;
    }
    else {
      settings = config.configDir() + val;
    }
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
  if ( host.empty() ){
    string init = "-s " + settings + " -vcf";
    tagger = new MbtAPI( init, *dbg_log );
    return tagger->isInit();
  }
  else {
    LOG << "using " << _label << "-tagger on "
	<< host << ":" << port << endl;
    return true;
  }
}

void BaseTagger::add_provenance( folia::Document& doc,
				 folia::processor *main ) const {
  if ( !main ){
    throw logic_error( _label + "::add_provenance() without parent proc." );
  }
  folia::KWargs args;
  args["name"] = _label;
  args["generate_id"] = "auto()";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  folia::processor *proc = doc.add_processor( args, main );
  add_declaration( doc, proc );
}

json create_json( const vector<tag_entry>& tv ){
  json result;
  result["command"] = "tag";
  json arr = json::array();
  for ( const auto& it : tv ){
    json one_entry;
    one_entry["word"] = it.word;
    if ( !it.enrichment.empty() ){
      one_entry["enrichment"] = it.enrichment;
    }
    arr.push_back( one_entry );
  }
  result["sentence"] = arr;
  return result;
}

vector<TagResult> json_to_TR( const json& in ){
  vector<TagResult> result;
  for ( auto& i : in ){
    TagResult tr;
    tr.set_word( i["word"] );
    if ( i.find("known") != i.end() ){
      tr.set_known( i["known"] == "true" );
    }
    tr.set_tag( i["tag"] );
    if ( i.find("confidence") != i.end() ){
      tr.set_confidence( i["confidence"] );;
    }
    if ( i.find("distance") != i.end() ){
      tr.set_distance( i["distance"] );
    }
    if ( i.find("distribution") != i.end() ){
      tr.set_distribution( i["distribution"] );
    }
    if ( i.find("enrichment") != i.end() ){
      tr.set_enrichment( i["enrichment"] );
    }
    result.push_back( tr );
  }
  return result;
}

vector<TagResult> BaseTagger::call_server( const vector<tag_entry>& tv ) const {
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    LOG << "failed to open connection, " << _label << "::" << host
	<< ":" << port << endl
	<< "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  DBG << "calling " << _label << "-server, base=" << base << endl;
  string line;
  client.read( line );
  json response;
  try {
    response = json::parse( line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << line << "':"
	<< e.what() << endl;
    abort();
  }
  DBG << "got JSON " << response.dump(2) << endl;
  if ( response["status"] != "ok" ){
    LOG << "the client isn't OK" << endl;
    abort();
  }
  if ( !base.empty() ){
    json out_json;
    out_json["command"] = "base";
    out_json["param"] = base;
    string line = out_json.dump() + "\n";
    DBG << "sending BASE json data:" << line << endl;
    client.write( line );
    client.read( line );
    DBG << "received base data:" << line << endl;
    json base_response;
    try {
      base_response = json::parse( line );
    }
    catch ( const exception& e ){
      LOG << "json parsing failed on '" << line << "':"
      	  << e.what() << endl;
      abort();
    }
    DBG << "received json response:" << base_response << endl;
    if ( response["status"] != "ok" ){
      LOG << "the client isn't OK" << endl;
      abort();
    }
  }
  // create json query struct
  json my_json = create_json( tv );
  DBG << "created json" << my_json << endl;
  // send it to the server
  line = my_json.dump() + "\n";
  DBG << "sending json data:" << line << endl;
  client.write( line );
  // receive json
  client.read( line );
  DBG << "received line:" << line << "" << endl;
  try {
    my_json = json::parse( line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << line << "':"
	<< e.what() << endl;
    abort();
  }
  DBG << "received json data:" << my_json << endl;
  return json_to_TR( my_json );
}

vector<TagResult> BaseTagger::tagLine( const string& line ){
  if ( !host.empty() ){
    vector<tag_entry> to_do;
    vector<string> v = TiCC::split( line );
    for ( const auto& w : v ){
      icu::UnicodeString word = TiCC::UnicodeFromUTF8(w);
      if ( filter ){
	word = filter->filter( word );
      }
      string word_s = TiCC::UnicodeToUTF8( word );
      // the word may contain spaces, remove them all!
      word_s.erase(remove_if(word_s.begin(), word_s.end(), ::isspace), word_s.end());
      tag_entry entry;
      entry.word = word_s;
      to_do.push_back( entry );
    }
    return tagLine( to_do );
  }
  else if ( !tagger ){
    throw runtime_error( _label + "-tagger is not initialized" );
  }
  if ( debug > 1 ){
    DBG << "TAGGING LINE: " << line << endl;
  }
  return tagger->TagLine( line );
}

vector<TagResult> BaseTagger::tagLine( const vector<tag_entry>& to_do ){
  //  if ( debug > 1 ){
  DBG << "TAGGING TEXT_BLOCK\n" << endl;
  for ( const auto& it : to_do ){
    DBG << it.word << "\t" << it.enrichment << "\t" << endl;
  }
  //  }
  if ( !host.empty() ){
    DBG << "calling server" << endl;
    return call_server(to_do);
  }
  else {
    if ( !tagger ){
      throw runtime_error( _label + "-tagger is not initialized" );
    }
    string block;
    for ( const auto& e: to_do ){
      block += e.word;
      if ( !e.enrichment.empty() ){
	block += "\t" + e.enrichment;
	block += "\t??\n";
      }
      else {
	block += " ";
      }
    }
    block += "<utt>\n"; // should use tagger.eosmark??
    return tagger->TagLine( block );
  }
}

string BaseTagger::set_eos_mark( const std::string& eos ){
  if ( !host.empty() ){
    // just ignore??
    LOG << "cannot change EOS mark for external server!" << endl;
    return "";
  }
  if ( tagger ){
    return tagger->set_eos_mark( eos );
  }
  throw runtime_error( _label + "-tagger is not initialized" );
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

vector<tag_entry> BaseTagger::extract_sentence( const frog_data& sent ){
  vector<tag_entry> result;
  for ( const auto& sword : sent.units ){
    icu::UnicodeString word = TiCC::UnicodeFromUTF8(sword.word);
    if ( filter ){
      word = filter->filter( word );
    }
    string word_s = TiCC::UnicodeToUTF8( word );
    // the word may contain spaces, remove them all!
    word_s.erase(remove_if(word_s.begin(), word_s.end(), ::isspace), word_s.end());
    tag_entry entry;
    entry.word = word_s;
    result.push_back( entry );
  }
  return result;
}

ostream& operator<<( ostream&os, const tag_entry& e ){
  os << e.word;
  if ( !e.enrichment.empty() ){
    os << "\t" << e.enrichment;
  }
  os << endl;
  return os;
}

void BaseTagger::Classify( frog_data& sent ){
  _words.clear();
  vector<tag_entry> to_do = extract_sentence( sent );
  if ( debug > 1 ){
    DBG << _label << "-tagger in: " << to_do << endl;
  }
  _tag_result = tagLine( to_do );
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
	  << " tag=" << _tag_result[i].assigned_tag()
	  << " confidence=" << _tag_result[i].confidence() << endl;
    }
  }
  post_process( sent );
}
