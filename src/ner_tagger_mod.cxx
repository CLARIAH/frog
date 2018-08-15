/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2018
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

#include "frog/ner_tagger_mod.h"

#include "mbt/MbtAPI.h"
#include "frog/Frog-util.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"

using namespace std;
using namespace Tagger;
using TiCC::operator<<;

#define LOG *TiCC::Log(tag_log)

static string POS_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";

NERTagger::NERTagger( TiCC::LogStream *l ):
  BaseTagger( l, "NER" ),
  max_ner_size(20)
{ known_ners.resize( max_ner_size + 1 );
  override_ners.resize( max_ner_size + 1 );
}

bool NERTagger::init( const TiCC::Configuration& config ){
  if ( !BaseTagger::init( config ) ){
    return false;
  }
  string val = config.lookUp( "max_ner_size", "NER" );
  if ( !val.empty() ){
    max_ner_size = TiCC::stringTo<int>( val );
  }
  val = config.lookUp( "known_ners", "NER" );
  if ( !val.empty() ){
    if ( !read_gazets( val, config.configDir() ) ){
      return false;
    }
  }
  val = config.lookUp( "ner_override", "NER" );
  if ( !val.empty() ){
    if ( !read_overrides( val, config.configDir() ) ){
      return false;
    }
  }
  val = config.lookUp( "set", "tagger" );
  if ( !val.empty() ){
    POS_tagset = val;
  }
  return true;
}

bool NERTagger::fill_ners( const string& cat,
			   const string& name,
			   const string& config_dir,
			   vector<unordered_map<string,set<string>>>& ners ){
  string file_name = name;
  if ( !TiCC::isFile( file_name ) ){
    file_name = config_dir + "/" + name;
    if ( !TiCC::isFile( file_name ) ){
      LOG << "unable to load additional NE from file: " << file_name << endl;
      return false;
    }
  }
  ifstream is( file_name );
  int long_err_cnt = 0;
  size_t ner_cnt = 0;
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    else {
      vector<string> parts = TiCC::split( line );
      if ( parts.size() > (unsigned)max_ner_size ){
	// LOG << "expected 1 to " << max_ner_size
	// 		   << " SPACE-separated parts in line: '" << line
	// 		   << "'" << endl;
	if ( ++long_err_cnt > 50 ){
	  LOG << "too many long entries in additional wordlist file. " << file_name << endl;
	  LOG << "consider raising the max_ner_size in the configuration. (now "
	      << max_ner_size << ")" << endl;
	  return false;
	}
	else {
	//	LOG << "ignoring entry" << endl;
	  continue;
	}
      }
      // reconstruct the NER with single spaces
      line = "";
      for ( const auto& part : parts ){
	line += part;
	if ( &part != &parts.back() ){
	  line += " ";
	}
      }
      ners[parts.size()][line].insert( cat );
      ++ner_cnt;
    }
  }
  LOG << "loaded " << ner_cnt << " additional " << cat
      << " Named Entities from: " << file_name << endl;
  return true;
}

bool NERTagger::read_gazets( const string& name,
			     const string& config_dir,
			     vector<unordered_map<string,set<string>>>& ners ){
  string file_name = name;
  string lookup_dir = config_dir;
  if ( name[0] != '/' ) {
    file_name = config_dir + "/" + file_name;
  }
  else {
    lookup_dir = TiCC::dirname( file_name );
  }
  ifstream is( file_name );
  if ( !is ){
    LOG << "Unable to find Named Enties file " << file_name << endl;
    return false;
  }
  LOG << "READ  " << file_name << endl;
  int err_cnt = 0;
  size_t file_cnt = 0;
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    // we search for entries of the form 'category\tfilename'
    vector<string> parts;
    if ( TiCC::split_at( line, parts, "\t" ) != 2 ){
      LOG << "expected 2 TAB-separated parts in line: '" << line << "'" << endl;
      if ( ++err_cnt > 50 ){
	LOG << "too many errors in additional wordlist file: " << file_name << endl;
	return false;
      }
      else {
	LOG << "ignoring entry" << endl;
	continue;
      }
    }
    string cat  = parts[0];
    string file = parts[1];
    if ( fill_ners( cat, file, lookup_dir, ners ) ){
      ++file_cnt;
    }
  }
  if ( file_cnt < 1 ){
    LOG << "unable to load any additional Named Enties." << endl;
    return false;
  }
  else {
    LOG << "loaded " << file_cnt << " additional Named Entities files" << endl;
    return true;
  }
}

static vector<string> serialize( const vector<set<string>>& stags ){
  // for every non empty set {el1,el2,..}, we compose a string like: el1+el2+...
  vector<string> ambitags( stags.size(), "O" );
  size_t pos = 0;
  for ( const auto& it : stags ){
    if ( !it.empty() ){
      string res;
      for ( const auto& s : it ){
	res += s + "+";
      }
      ambitags[pos] = res;
    }
    ++pos;
  }
  return ambitags;
}

vector<string> NERTagger::create_ner_list( const vector<string>& words,
					   std::vector<std::unordered_map<std::string,std::set<std::string>>>& ners ){
  vector<set<string>> stags( words.size() );
  if ( debug > 1 ){
    LOG << "search for known NER's" << endl;
  }
  for ( size_t j=0; j < words.size(); ++j ){
    // cycle through the words
    string seq;
    size_t len = 1;
    for ( size_t i = 0; i < min( words.size() - j, (size_t)max_ner_size); ++i ){
      // start looking for sequences of length len
      auto const& mp = ners[len++];
      if ( mp.empty() ){
	continue;
      }
      seq += words[j+i];
      if ( debug > 1 ){
	LOG << "sequence = '" << seq << "'" << endl;
      }
      auto const& tags = mp.find(seq);
      if ( tags != mp.end() ){
	if ( debug > 1 ){
	  LOG << "FOUND tags " << tags->first << "-" << tags->second << endl;
	}
	for ( size_t k = 0; k <= i; ++k ){
	  stags[k+j].insert( tags->second.begin(), tags->second.end() );
	}
      }
      seq += " ";
    }
  }
  return serialize( stags );
}

void NERTagger::addEntity( folia::Sentence *sent,
			   const vector<folia::Word*>& words,
			   const vector<double>& confs,
			   const string& NER ){
  folia::EntitiesLayer *el = 0;
  if ( debug > 8 ){
    LOG << "add NER " << NER << " to entities layer" << endl;
  }

#pragma omp critical (foliaupdate)
  {
    try {
      el = sent->annotation<folia::EntitiesLayer>(tagset);
      if ( debug > 8 ){
	LOG << "GOT existing layer " << el << endl;
      }
    }
    catch(...){
      folia::KWargs args;
      string s_id = sent->id();
      if ( !s_id.empty() ){
	args["generate_id"] = s_id;
      }
      args["set"] = tagset;
      el = new folia::EntitiesLayer( args, sent->doc() );
      if ( debug > 8 ){
	LOG << "CREATED layer " << el << endl;
      }
      sent->append( el );
    }
  }
  double c = 0;
  for ( auto const& val : confs ){
    c += val;
  }
  c /= confs.size();
  folia::KWargs args;
  args["class"] = NER;
  args["confidence"] = TiCC::toString(c);
  args["set"] = tagset;
  string el_id = el->id();
  if ( !el_id.empty() ){
    args["generate_id"] = el_id;
  }
  if ( textclass != "current" ){
    args["textclass"] = textclass;
  }
  folia::Entity *e = 0;
#pragma omp critical (foliaupdate)
  {
    e = new folia::Entity( args, el->doc() );
    el->append( e );
  }
  for ( const auto& word : words ){
#pragma omp critical (foliaupdate)
    {
      e->append( word );
    }
  }
}

void NERTagger::addNERTags( const vector<folia::Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.empty() ) {
    return;
  }
  folia::Sentence *sent = words[0]->sentence();
  vector<folia::Word*> stack;
  vector<double> dstack;
  string curNER;
  for ( size_t i=0; i < tags.size(); ++i ){
    if (debug > 1){
      LOG << "NER = " << tags[i] << endl;
    }
    vector<string> ner;
    if ( tags[i] == "O" ){
      if ( !stack.empty() ){
	if (debug > 1) {
	  LOG << "O spit out " << curNER << endl;
	  LOG << "ners  " << stack << endl;
	  LOG << "confs " << dstack << endl;
	}
	addEntity( sent, stack, dstack, curNER );
	dstack.clear();
	stack.clear();
      }
      continue;
    }
    else {
      size_t num_words = TiCC::split_at( tags[i], ner, "-" );
      if ( num_words != 2 ){
	LOG << "expected <NER>-tag, got: " << tags[i] << endl;
	throw runtime_error( "NER: unable to retrieve a NER tag from: "
			     + tags[i] );
      }
    }
    if ( ner[0] == "B" ||
	 ( ner[0] == "I" && stack.empty() ) ||
	 ( ner[0] == "I" && ner[1] != curNER ) ){
      // an I without preceding B is handled as a B
      // an I with a different TAG is also handled as a B
      if ( !stack.empty() ){
	if ( debug > 1 ){
	  LOG << "B spit out " << curNER << endl;
	  LOG << "spit out " << stack << endl;
	}
	addEntity( sent, stack, dstack, curNER );
	dstack.clear();
	stack.clear();
      }
      curNER = ner[1];
    }
    dstack.push_back( confs[i] );
    stack.push_back( words[i] );
  }
  if ( !stack.empty() ){
    if ( debug > 1 ){
      LOG << "END spit out " << curNER << endl;
      LOG << "spit out " << stack << endl;
    }
    addEntity( sent, stack, dstack, curNER );
  }
}

void NERTagger::addDeclaration( folia::Document& doc ) const {
  doc.declare( folia::AnnotationType::ENTITY,
	       tagset,
	       "annotator='frog-ner-" + _version
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
}

void NERTagger::Classify( const vector<folia::Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    vector<string> ptags;
    extract_words_tags( swords, POS_tagset, words, ptags );
    vector<string> ktags = create_ner_list( words, known_ners );
    vector<string> override_tags = create_ner_list( words, override_ners );
    string text_block;
    string prev = "_";
    string prevN = "_";
    for ( size_t i=0; i < swords.size(); ++i ){
      string word = words[i];
      string pos = ptags[i];
      text_block += word + "\t" + prev + "\t" + pos + "\t";
      prev = pos;
      if ( i < swords.size() - 1 ){
	text_block += ptags[i+1];
      }
      else {
	text_block += "_";
      }
      string ktag = ktags[i];
      text_block += "\t" + prevN + "\t" + ktag + "\t";
      prevN = ktag;
      if ( i < swords.size() - 1 ){
	text_block += ktags[i+1];
      }
      else {
	text_block += "_";
      }

      text_block += "\t??\n";
    }
    if ( debug > 1 ){
      LOG << "TAGGING TEXT_BLOCK\n" << text_block << endl;
    }
    _tag_result = tagger->TagLine( text_block );
    if ( debug > 1 ){
      LOG << "NER tagger out: " << endl;
      for ( size_t i=0; i < _tag_result.size(); ++i ){
	LOG << "[" << i << "] : word=" << _tag_result[i].word()
	    << " tag=" << _tag_result[i].assignedTag()
	    << " confidence=" << _tag_result[i].confidence() << endl;
      }
    }
    post_process( swords, override_tags );
  }
}

void NERTagger::post_process( const vector<folia::Word*>& ){
  throw logic_error( "NER tagger call undefined postprocess() member" );
}

void NERTagger::Classify( frog_data& swords ){
  if ( debug ){
    LOG << "classify from DATA" << endl;
  }
  vector<string> words;
  vector<string> ptags;
#pragma omp critical (dataupdate)
  {
    for ( const auto& w : swords.units ){
      words.push_back( w.word );
      ptags.push_back( w.tag );
    }
  }
  vector<string> ktags = create_ner_list( words, known_ners );
  vector<string> override_tags = create_ner_list( words, override_ners );
  string text_block;
  string prev = "_";
  string prevN = "_";
  for ( size_t i=0; i < swords.size(); ++i ){
    string word = words[i];
    string pos = ptags[i];
    text_block += word + "\t" + prev + "\t" + pos + "\t";
    prev = pos;
    if ( i < swords.size() - 1 ){
      text_block += ptags[i+1];
    }
    else {
      text_block += "_";
    }
    string ktag = ktags[i];
    text_block += "\t" + prevN + "\t" + ktag + "\t";
    prevN = ktag;
    if ( i < swords.size() - 1 ){
      text_block += ktags[i+1];
    }
    else {
      text_block += "_";
    }

    text_block += "\t??\n";
  }
  if ( debug > 1 ){
    LOG << "TAGGING TEXT_BLOCK\n" << text_block << endl;
  }
  _tag_result = tagger->TagLine( text_block );
  if ( debug > 1 ){
    LOG << "NER tagger out: " << endl;
    for ( size_t i=0; i < _tag_result.size(); ++i ){
      LOG << "[" << i << "] : word=" << _tag_result[i].word()
	  << " tag=" << _tag_result[i].assignedTag()
	  << " confidence=" << _tag_result[i].confidence() << endl;
    }
  }
  post_process( swords, override_tags );
}

void NERTagger::post_process( frog_data& swords,
			      const vector<string>& override ){
  vector<string> tags;
  vector<double> conf;
  string last_tag;
  for ( const auto& tag : _tag_result ){
    string a_tag = tag.assignedTag();
    if ( a_tag[0] == 'I' ){
      // make sure that we start a new 'sequence' with a B
      if ( last_tag.empty() ){
	a_tag[0] = 'B';
      }
      else {
	if ( last_tag.substr(2) != a_tag.substr( 2 ) ){
	  a_tag[0] = 'B';
	}
      }
      last_tag = a_tag;
    }
    else if ( a_tag[0] == 'O' ){
      last_tag.clear();
    }
    else if ( a_tag[0] == 'B' ){
      last_tag = a_tag;
    }
    tags.push_back( a_tag );
    conf.push_back( tag.confidence() );
  }
  if ( !override.empty() ){
    vector<string> empty;
    merge_override( tags, conf, override, true, empty );
  }
  addNERTags( swords, tags, conf );
}

void NERTagger::addNERTags( frog_data& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.size() == 0 ) {
    return;
  }
  vector<string> stack;
  vector<double> dstack;
  string curNER;
  for ( size_t i=0; i < tags.size(); ++i ){
    if (debug > 1){
      LOG << "NER = " << tags[i] << endl;
    }
    vector<string> ner;
    if ( tags[i] == "O" ){
      if ( !stack.empty() ){
	if (debug > 1) {
	  LOG << "O spit out " << curNER << endl;
	  LOG << "ners  " << stack << endl;
	  LOG << "confs " << dstack << endl;
	}
	addEntity( words, i, stack, dstack );
	dstack.clear();
	stack.clear();
      }
      continue;
    }
    else {
      size_t num_words = TiCC::split_at( tags[i], ner, "-" );
      if ( num_words != 2 ){
	LOG << "expected <NER>-tag, got: " << tags[i] << endl;
	throw runtime_error( "NER: unable to retrieve a NER tag from: "
			     + tags[i] );
      }
    }
    if ( ner[0] == "B" ||
	 ( ner[0] == "I" && stack.empty() ) ||
	 ( ner[0] == "I" && ner[1] != curNER ) ){
      // an I without preceding B is handled as a B
      // an I with a different TAG is also handled as a B
      if ( !stack.empty() ){
	if ( debug > 1 ){
	  LOG << "B spit out " << curNER << endl;
	  LOG << "spit out " << stack << endl;
	}
	addEntity( words, i, stack, dstack );
	dstack.clear();
	stack.clear();
      }
      curNER = ner[1];
    }
    dstack.push_back( confs[i] );
    stack.push_back( tags[i] );
  }
  if ( !stack.empty() ){
    if ( debug > 1 ){
      LOG << "END spit out " << curNER << endl;
      LOG << "spit out " << stack << endl;
    }
    addEntity( words, words.size(), stack, dstack );
  }
}

void NERTagger::addEntity( frog_data& sent,
			   const size_t pos,
			   const vector<string>& words,
			   const vector<double>& confs ){
  double c = 0;
  for ( auto const& val : confs ){
    c += val;
  }
  c /= confs.size();
  for ( size_t i = 0; i < words.size(); ++i ){
#pragma omp critical (foliaupdate)
    {
      sent.units[pos-words.size()+i].ner_tag = words[i];
      sent.units[pos-words.size()+i].ner_confidence = c;
    }
  }
}


void NERTagger::post_process( frog_data& ){
  throw logic_error( "NER tagger call undefined postprocess() member" );
}

string to_tag( const string& label, bool inside ){
  vector<string> parts = TiCC::split_at( label, "+" );
  if ( parts.size() > 1 ){
    // undecided
    return "O";
  }
  else {
    string result;
    if ( inside ){
      result += "I-";
    }
    else {
      result += "B-";
    }
    result += parts[0];
    return result;
  }
}

void NERTagger::merge_override( vector<string>& tags,
				vector<double>& conf,
				const vector<string>& override,
				bool unconditional,
				const vector<string>& POS_tags ) const{
  string label;
  for ( size_t i=0; i < tags.size(); ++i ){
    if ( override[i] != "O"
	 && ( POS_tags.empty()
	      || POS_tags[i].find("SPEC(") != string::npos
	      || POS_tags[i].find("N(") != string::npos ) ){
      // if ( i == 0 ){
      //  	 cerr << "override = " << override << endl;
      //  	 cerr << "ner tags = " << tags << endl;
      //  	 cerr << "POS tags = " << POS_tags << endl;
      // }
      if ( tags[i][0] != 'O'
	   && !unconditional ){
	// don't tamper with existing tags
	continue;
      }
      bool inside = (label == override[i] );
      //      cerr << "step i=" << i << " override=" << override[i] << endl;
      string replace = to_tag(override[i], inside );
      //      cerr << "replace=" << replace << endl;
      if ( replace != "O" ){
	// there is something to override.
	// NB: replace wil return "O" for ambi tags too!
	if ( tags[i][0] == 'I' && !inside ){
	  //	  cerr << "before  whiping tags = " << tags << endl;
	  //oops, inside a NER tag, and now a new override
	  // whipe previous I tags, and one B
	  if ( i == 0 ){
	    // strange exception, in fact starting with an I tag is impossible
	    tags[i][0] = 'B'; // fix it on the fly
	    continue; // next i
	  }
	  for ( size_t j = i-1; j > 0; --j ){
	    if ( tags[j][0] == 'B' ){
	      // we are done
	      tags[j] = "O";
	      break;
	    }
	    tags[j] = "O";
	  }
	  // whipe next I tags too, to be sure
	  for ( size_t j = i+1; j < tags.size(); ++j ){
	    if ( tags[j][0] != 'I' ){
	      // a B or O
	      break;
	    }
	    // still inside
	    tags[j] = "O";
	  }
	}
	//	cerr << POS_tags[i] << "REPLACE " << tags[i] << " by " << replace << endl;
	tags[i] = replace;
	conf[i] = 1;
	if ( !inside ){
	  label = override[i];
	}
      }
    }
    else {
      label = "";
    }
  }
}

void NERTagger::post_process( const vector<folia::Word*>& swords,
			      const vector<string>& override ){
  vector<string> tags;
  vector<double> conf;
  for ( const auto& tag : _tag_result ){
    tags.push_back( tag.assignedTag() );
    conf.push_back( tag.confidence() );
  }
  if ( !override.empty() ){
    vector<string> empty;
    merge_override( tags, conf, override, true, empty );
  }
  addNERTags( swords, tags, conf );
}

bool NERTagger::Generate( const std::string& opt_line ){
  return tagger->GenerateTagger( opt_line );
}

void NERTagger::add_result( folia::Sentence *s,
			    const frog_data& fd,
			    const vector<folia::Word*>& wv ) const {
  folia::EntitiesLayer *el = 0;
  folia::Entity *ner = 0;
  size_t i = 0;
  for ( const auto& word : fd.units ){
    if ( word.ner_tag[0] == 'B' ){
      if ( el == 0 ){
	// create a layer, we need it
	folia::KWargs args;
	args["set"] = getTagset();
	args["generate_id"] = s->id();
	el = new folia::EntitiesLayer( args, s->doc() );
	s->append(el);
      }
      // a new entity starts here
      if ( ner != 0 ){
	el->append( ner );
      }
      // now make new entity
      folia::KWargs args;
      args["set"] = getTagset();
      args["generate_id"] = el->id();
      args["class"] = word.ner_tag.substr(2);
      args["confidence"] = TiCC::toString(word.ner_confidence);
      ner = new folia::Entity( args, s->doc() );
      ner->append( wv[i] );
    }
    else if ( word.ner_tag[0] == 'I' ){
      // continue in an entity
      assert( ner != 0 );
      ner->append( wv[i] );
    }
    else if ( word.ner_tag[0] == '0' ){
      if ( ner != 0 ){
	el->append( ner );
	ner = 0;
      }
    }
    ++i;
  }
  if ( ner != 0 ){
    // some leftovers
    el->append( ner );
  }
}
