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

#include "frog/cgn_tagger_mod.h"
#include "frog/FrogData.h"
#include "frog/Frog-util.h"
#include "ticcutils/PrettyPrint.h"

using namespace std;
using namespace Tagger;
using TiCC::operator<<;

#define LOG *TiCC::Log(err_log)
#define DBG *TiCC::Log(dbg_log)


static string subsets_file = "subsets.cgn";
static string constraints_file = "constraints.cgn";

bool CGNTagger::fillSubSetTable( const string& sub_file,
				 const string& const_file ){
  ifstream sis( sub_file );
  if ( !sis ){
    LOG << "unable to open subsets file: " << sub_file << endl;
    return false;
  }
  LOG << "reading subsets from " << sub_file << endl;
  string line;
  while ( getline( sis, line ) ){
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> att_val = TiCC::split_at( line, "=", 2 );
    if ( att_val.size() != 2 ){
      LOG << "invalid line in:'" << sub_file << "' : " << line << endl;
      return false;
    }
    string at = TiCC::trim(att_val[0]);
    vector<string> vals = TiCC::split_at( att_val[1], "," );
    for ( const auto& val : vals ){
      cgnSubSets.insert( make_pair( TiCC::trim(val), at ) );
    }
  }
  if ( !const_file.empty() ){
    ifstream cis( const_file );
    if ( !cis ){
      LOG << "unable to open constraints file: " << const_file << endl;
      return false;
    }
    LOG << "reading constraints from " << const_file << endl;
    while ( getline( cis, line ) ){
      if ( line.empty() || line[0] == '#' )
	continue;
      vector<string> att_val = TiCC::split_at( line, "=", 2 );
      if ( att_val.size() != 2 ){
	LOG << "invalid line in:'" << sub_file << "' : " << line << endl;
	return false;
      }
      string at = TiCC::trim(att_val[0]);
      vector<string> vals = TiCC::split_at( att_val[1], "," );
      for ( const auto& val : vals ){
	cgnConstraints.insert( make_pair( at, TiCC::trim(val) ) );
      }
    }
  }
  return true;
}

bool CGNTagger::init( const TiCC::Configuration& config ){
  if (  debug > 1 ){
    DBG << "INIT CGN Tagger." << endl;
  }
  if ( !BaseTagger::init( config ) ){
    return false;
  }
  string val = config.lookUp( "subsets_file", "tagger" );
  if ( !val.empty() ){
    subsets_file = val;
  }
  if ( subsets_file != "ignore" ){
    subsets_file = prefix( config.configDir(), subsets_file );
  }
  val = config.lookUp( "constraints_file", "tagger" );
  if ( !val.empty() ){
    constraints_file = val;
  }
  if ( constraints_file != "ignore" && subsets_file == "ignore" ){
    LOG << "ERROR: when using a constraints file, you NEED a subsets file"
	<< endl;
    return false;
  }
  if ( constraints_file == "ignore" ){
    constraints_file.clear();
  }
  else {
    constraints_file = prefix( config.configDir(), constraints_file );
  }
  if ( subsets_file != "ignore" ){
    if ( !fillSubSetTable( subsets_file, constraints_file ) ){
      return false;
    }
  }
  if ( debug > 1 ){
    DBG << "DONE Init CGN Tagger." << endl;
  }
  return true;
}

void CGNTagger::addDeclaration( folia::Document& doc ) const {
  doc.declare( folia::AnnotationType::POS,
	       tagset,
	       "annotator='frog-mbpos-" + _version
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
}

void CGNTagger::addDeclaration( folia::Processor& proc ) const {
  proc.declare( folia::AnnotationType::POS,
		tagset,
		"annotator='frog-mbpos-" + _version
		+ "', annotatortype='auto', datetime='" + getTime() + "'");
}

string CGNTagger::getSubSet( const string& val, const string& head, const string& fullclass ) const {
  auto it = cgnSubSets.find( val );
  if ( it == cgnSubSets.end() ){
    throw folia::ValueError( "unknown cgn subset for class: '" + val + "', full class is: '" + fullclass + "'" );
  }
  string result;
  while ( it != cgnSubSets.upper_bound(val) ){
    result = it->second;
    auto cit = cgnConstraints.find( result );
    if ( cit == cgnConstraints.end() ){
      // no constraints on this value
      return result;
    }
    else {
      while ( cit != cgnConstraints.upper_bound( result ) ){
	if ( cit->second == head ) {
	  // allowed
	  return result;
	}
	++cit;
      }
    }
    ++it;
  }
  throw folia::ValueError( "unable to find cgn subset for class: '" + val +
			   "' within the constraints for '" + head + "', full class is: '" + fullclass + "'" );
}

void CGNTagger::post_process( frog_data& words ){
  for ( size_t i=0; i < _tag_result.size(); ++i ){
    addTag( words.units[i],
	    _tag_result[i].assignedTag(),
	    _tag_result[i].confidence() );
  }
}

void CGNTagger::addTag( frog_record& fd,
			const string& inputTag,
			double confidence ){
#pragma omp critical (dataupdate)
  {
    fd.tag = inputTag;
    if ( inputTag.find( "SPEC(" ) == 0 ){
      fd.tag_confidence = 1;
    }
    else {
      fd.tag_confidence = confidence;
    }
  }
  string ucto_class = fd.token_class;
  if ( debug > 1 ){
    DBG << "lookup ucto class= " << ucto_class << endl;
  }
  auto const tt = token_tag_map.find( ucto_class );
  if ( tt != token_tag_map.end() ){
    if ( debug > 1 ){
      DBG << "found translation ucto class= " << ucto_class
	  << " to POS-Tag=" << tt->second << endl;
    }
#pragma omp critical (dataupdate)
    {
      fd.tag = tt->second;
      fd.tag_confidence = 1.0;
    }
  }
}

void CGNTagger::add_tags( const vector<folia::Word*>& wv,
			  const frog_data& fd ) const {
  assert( wv.size() == fd.size() );
  size_t pos = 0;
  for ( const auto& word : fd.units ){
    folia::KWargs args;
    args["set"]   = getTagset();
    args["class"] = word.tag;
    if ( textclass != "current" ){
      args["textclass"] = textclass;
    }
    args["confidence"]= TiCC::toString(word.tag_confidence);
    folia::FoliaElement *postag;
#pragma omp critical (foliaupdate)
    {
      postag = wv[pos]->addPosAnnotation( args );
    }
    vector<string> hv = TiCC::split_at_first_of( word.tag, "()" );
    string head = hv[0];
    args["class"] = head;
#pragma omp critical (foliaupdate)
    {
      folia::Feature *feat = new folia::HeadFeature( args );
      postag->append( feat );
      if ( head == "SPEC" ){
	postag->confidence(1.0);
      }
    }
    vector<string> feats;
    if ( hv.size() > 1 ){
      feats = TiCC::split_at( hv[1], "," );
    }
    for ( const auto& f : feats ){
      folia::KWargs args;
      args["set"] =  getTagset();
      args["subset"] = getSubSet( f, head, word.tag );
      args["class"]  = f;
#pragma omp critical (foliaupdate)
      {
	folia::Feature *feat = new folia::Feature( args, wv[pos]->doc() );
	postag->append( feat );
      }
    }
    ++pos;
  }
}
