//
//   @@ All Rights Reserved @@
//  This file is part of the RDKit.
//  The contents are covered by the terms of the BSD license
//  which is included in the file license.txt, found at the root
//  of the RDKit source tree.
//
// Original author: David Cosgrove (AstraZeneca)
// 27th May 2014
//

#include <GraphMol/QueryOps.h>
#include <GraphMol/MolDraw2D/MolDraw2D.h>
#include <GraphMol/MolDraw2D/MolDraw2DDetails.h>

#include <cstdlib>
#include <limits>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/assign/list_of.hpp>

using namespace boost;
using namespace std;

namespace RDKit {

  // ****************************************************************************
  MolDraw2D::MolDraw2D( int width, int height ) :
  width_( width ) , height_( height ) , scale_( 1.0 ) , x_trans_( 0.0 ) ,
    y_trans_( 0.0 ) , font_size_( 0.5 ), curr_width_(2), fill_polys_(true) {
  }

  // ****************************************************************************
  void MolDraw2D::drawMolecule( const ROMol &mol ,
                                const vector<int> *highlight_atoms ,
                                const map<int,DrawColour> *highlight_atom_map,
                                const std::map<int,double> *highlight_radii,
                                int confId) {
    vector<int> highlight_bonds;
    if(highlight_atoms){
      for(vector<int>::const_iterator ai=highlight_atoms->begin();
          ai!=highlight_atoms->end();++ai){
        for(vector<int>::const_iterator aj=ai+1;
            aj!=highlight_atoms->end();++aj){
          const Bond *bnd=mol.getBondBetweenAtoms(*ai,*aj);
          if(bnd) highlight_bonds.push_back(bnd->getIdx());
        }
      }
    }
    drawMolecule(mol,highlight_atoms,&highlight_bonds,highlight_atom_map,NULL,highlight_radii,confId);
  }

  void MolDraw2D::doContinuousHighlighting( const ROMol &mol ,
                                            const vector<int> *highlight_atoms ,
                                            const vector<int> *highlight_bonds ,
                                            const map<int,DrawColour> *highlight_atom_map,
                                            const map<int,DrawColour> *highlight_bond_map,
                                            const std::map<int,double> *highlight_radii
                                            ) {
    int orig_lw=lineWidth();
    int tgt_lw=lineWidth()*8;
    // try to scale lw to reflect the overall scaling:
    tgt_lw = max(orig_lw*2,min(tgt_lw,(int)(scale_/25.*tgt_lw))); // the 25 here is extremely empirical
    bool orig_fp=fillPolys();
    ROMol::VERTEX_ITER this_at , end_at;
    if(highlight_bonds){
      boost::tie( this_at , end_at ) = mol.getVertices();
      while( this_at != end_at ) {
        int this_idx = mol[*this_at]->getIdx();
        ROMol::OEDGE_ITER nbr , end_nbr;
        boost::tie( nbr , end_nbr ) = mol.getAtomBonds( mol[*this_at].get() );
        while( nbr != end_nbr ) {
          const BOND_SPTR bond = mol[*nbr];
          ++nbr;
          int nbr_idx = bond->getOtherAtomIdx( this_idx );
          if( nbr_idx < static_cast<int>( at_cds_.size() ) && nbr_idx > this_idx ) {
            if(std::find(highlight_bonds->begin(),highlight_bonds->end(),bond->getIdx()) != highlight_bonds->end()){
              DrawColour col=drawOptions().highlightColour;
              if(highlight_bond_map &&
                 highlight_bond_map->find(bond->getIdx())!=highlight_bond_map->end()){
                col = highlight_bond_map->find(bond->getIdx())->second;
              }
              setLineWidth(tgt_lw);
              Point2D at1_cds = at_cds_[this_idx];
              Point2D at2_cds = at_cds_[nbr_idx];
              drawLine( at1_cds , at2_cds , col , col);
            }
          }
        }
        ++this_at;
      }
    }
    if(highlight_atoms){
      boost::tie( this_at , end_at ) = mol.getVertices();
      while( this_at != end_at ) {
        int this_idx = mol[*this_at]->getIdx();
        if(std::find(highlight_atoms->begin(),highlight_atoms->end(),this_idx) != highlight_atoms->end()){
          DrawColour col=drawOptions().highlightColour;
          if(highlight_atom_map &&
             highlight_atom_map->find(this_idx)!=highlight_atom_map->end()){
            col = highlight_atom_map->find(this_idx)->second;
          }
          Point2D p1=at_cds_[this_idx];
          Point2D p2=at_cds_[this_idx];
          double radius=0.4;
          if(highlight_radii && highlight_radii->find(this_idx)!=highlight_radii->end()){
            radius = highlight_radii->find(this_idx)->second;
          }
          Point2D offset(radius,radius);
          p1 -= offset;
          p2 += offset;
          setColour(col);
          setFillPolys(true);
          setLineWidth(1);
          drawEllipse(p1,p2);
        }
        ++this_at;
      }
    }
    setLineWidth(orig_lw);
    setFillPolys(orig_fp);
  }

  void MolDraw2D::drawMolecule( const ROMol &mol ,
                                const vector<int> *highlight_atoms ,
                                const vector<int> *highlight_bonds ,
                                const map<int,DrawColour> *highlight_atom_map,
                                const map<int,DrawColour> *highlight_bond_map,
                                const std::map<int,double> *highlight_radii,
                                int confId ) {
    if(drawOptions().clearBackground){
      clearDrawing();
    }
    extractAtomCoords( mol, confId );
    extractAtomSymbols( mol );
    calculateScale();
    setFontSize( font_size_ );

    if(drawOptions().includeAtomTags){
      tagAtoms(mol);
    }
    if(drawOptions().atomRegions.size()){
      BOOST_FOREACH(const std::vector<int> &region, drawOptions().atomRegions){
        if(region.size()>1){
          Point2D minv=at_cds_[region[0]];
          Point2D maxv=at_cds_[region[0]];
          BOOST_FOREACH(int idx,region){
            const Point2D &pt=at_cds_[idx];
            minv.x = std::min(minv.x,pt.x);
            minv.y = std::min(minv.y,pt.y);
            maxv.x = std::max(maxv.x,pt.x);
            maxv.y = std::max(maxv.y,pt.y);
          }
          Point2D center=(maxv+minv)/2;
          Point2D size=(maxv-minv);
          size *= 0.2;
          minv -= size/2;
          maxv += size/2;
          setColour(DrawColour(.8,.8,.8));
          //drawEllipse(minv,maxv);
          drawRect(minv,maxv);
        }
      }
    }
    
    if(drawOptions().continuousHighlight){
      // if we're doing continuous highlighting, start by drawing the highlights
      doContinuousHighlighting(mol,highlight_atoms,highlight_bonds,
                               highlight_atom_map,highlight_bond_map,highlight_radii);
      // at this point we shouldn't be doing any more higlighting, so blow out those variables:
      highlight_bonds=NULL;
      highlight_atoms=NULL;
    } else if(drawOptions().circleAtoms && highlight_atoms){
      ROMol::VERTEX_ITER this_at , end_at;
      boost::tie( this_at , end_at ) = mol.getVertices();
      setFillPolys(false);
      while( this_at != end_at ) {
        int this_idx = mol[*this_at]->getIdx();
        if(std::find(highlight_atoms->begin(),highlight_atoms->end(),this_idx) != highlight_atoms->end()){
          if(highlight_atom_map && highlight_atom_map->find(this_idx)!=highlight_atom_map->end()){
            setColour(highlight_atom_map->find(this_idx)->second);
          } else {
            setColour(drawOptions().highlightColour);
          }
          Point2D p1=at_cds_[this_idx];
          Point2D p2=at_cds_[this_idx];
          double radius=0.3;
          if(highlight_radii && highlight_radii->find(this_idx)!=highlight_radii->end()){
            radius = highlight_radii->find(this_idx)->second;
          }
          Point2D offset(radius,radius);
          p1 -= offset;
          p2 += offset;
          drawEllipse(p1,p2);
        }
        ++this_at;
      }
      setFillPolys(true);
    }


    ROMol::VERTEX_ITER this_at , end_at;
    boost::tie( this_at , end_at ) = mol.getVertices();
    while( this_at != end_at ) {
      int this_idx = mol[*this_at]->getIdx();
      ROMol::OEDGE_ITER nbr , end_nbr;
      boost::tie( nbr , end_nbr ) = mol.getAtomBonds( mol[*this_at].get() );
      while( nbr != end_nbr ) {
        const BOND_SPTR bond = mol[*nbr];
        ++nbr;
        int nbr_idx = bond->getOtherAtomIdx( this_idx );
        if( nbr_idx < static_cast<int>( at_cds_.size() ) && nbr_idx > this_idx ) {
          drawBond( mol , bond , this_idx , nbr_idx , highlight_atoms ,
                    highlight_atom_map, highlight_bonds, highlight_bond_map );
        }
      }
      ++this_at;
    }

    if(drawOptions().dummiesAreAttachments){
      ROMol::VERTEX_ITER atom , end_atom;
      boost::tie( atom , end_atom ) = mol.getVertices();
      while( atom != end_atom ) {
        const Atom *at1 = mol[*atom].get();
        ++atom;
        if(drawOptions().atomLabels.find(at1->getIdx()) != drawOptions().atomLabels.end() ){
          // skip dummies that explicitly have a label provided
          continue;
        }
        if(at1->getAtomicNum()==0 && at1->getDegree()==1){
          Point2D &at1_cds = at_cds_[at1->getIdx()];
          ROMol::ADJ_ITER nbrIdx,endNbrs;
          boost::tie(nbrIdx,endNbrs) = mol.getAtomNeighbors(at1);
          const ATOM_SPTR at2 = mol[*nbrIdx];
          Point2D &at2_cds = at_cds_[at2->getIdx()];
          drawAttachmentLine(at2_cds,at1_cds,DrawColour(.5,.5,.5));
        }
      }
    }
    
    for( int i = 0 , is = atom_syms_.size() ; i < is ; ++i ) {
      if( !atom_syms_[i].first.empty() ) {
        drawAtomLabel( i , highlight_atoms , highlight_atom_map );
      }
    }

    if(drawOptions().flagCloseContactsDist>=0){
      highlightCloseContacts();
    }
  }

  void MolDraw2D::highlightCloseContacts(){
    if(drawOptions().flagCloseContactsDist<0) return;
    int tol=drawOptions().flagCloseContactsDist * drawOptions().flagCloseContactsDist;
    boost::dynamic_bitset<> flagged(at_cds_.size());
    for(unsigned int i=0;i<at_cds_.size();++i){
      if(flagged[i]) continue;
      Point2D ci=getDrawCoords(at_cds_[i]);
      for(unsigned int j=i+1;j<at_cds_.size();++j){
        if(flagged[j]) continue;
        Point2D cj=getDrawCoords(at_cds_[j]);
        double d=(cj-ci).lengthSq();
        if( d<= tol ){
          flagged.set(i);
          flagged.set(j);
          break;
        }
      }
      if(flagged[i]){
        Point2D p1=at_cds_[i];
        Point2D p2=p1;
        Point2D offset(0.1,0.1);
        p1 -= offset;
        p2 += offset;
        bool ofp=fillPolys();
        setFillPolys(false);
        DrawColour odc=colour();
        setColour(DrawColour(1,0,0));
        drawRect(p1,p2);
        setColour(odc);
        setFillPolys(ofp);
      }
    }
  }
  
  // ****************************************************************************
  // transform a set of coords in the molecule's coordinate system
  // to drawing system coordinates
  Point2D MolDraw2D::getDrawCoords( const Point2D &mol_cds ) const {
    double x = scale_ * ( mol_cds.x - x_min_ + x_trans_ );
    double y = scale_ * ( mol_cds.y - y_min_ + y_trans_ );

    return Point2D( x , y );
  }

  // ****************************************************************************
  Point2D MolDraw2D::getDrawCoords( int at_num ) const {
    return getDrawCoords( at_cds_[at_num] );
  }

  // ****************************************************************************
  Point2D MolDraw2D::getAtomCoords( const pair<int, int> &screen_cds) const {
    int x = int( double( screen_cds.first ) / scale_ + x_min_ - x_trans_ );
    int y = int( double( screen_cds.second ) / scale_ + y_min_ - y_trans_ );

    return Point2D( x , y );
  }

  // ****************************************************************************
  Point2D MolDraw2D::getAtomCoords( int at_num ) const {
    return at_cds_[at_num];
  }

  // ****************************************************************************
  void MolDraw2D::setFontSize( double new_size ) {
    font_size_ = new_size;
  }

  // ****************************************************************************
  void MolDraw2D::calculateScale() {
    x_min_ = y_min_ = numeric_limits<double>::max();
    double x_max( -numeric_limits<double>::max() ) , y_max( -numeric_limits<double>::max() );
    for( int i = 0 , is = at_cds_.size() ; i < is ; ++i ) {
      const Point2D &pt = at_cds_[i];
      x_min_ = std::min( pt.x , x_min_ );
      y_min_ = std::min( pt.y , y_min_ );
      x_max = std::max( pt.x , x_max );
      y_max = std::max( pt.y , y_max );
    }

    x_range_ = x_max - x_min_;
    y_range_ = y_max - y_min_;
    scale_ = std::min( double( width_ ) / x_range_ , double( height_ ) / y_range_ );

    // we may need to adjust the scale if there are atom symbols that go off
    // the edges, and we probably need to do it iteratively because get_string_size
    // uses the current value of scale_.
    while( 1 ) {
      for( int i = 0 , is = atom_syms_.size() ; i < is ; ++i ) {
        if( !atom_syms_[i].first.empty() ) {
          double atsym_width , atsym_height;
          getStringSize( atom_syms_[i].first , atsym_width , atsym_height );
          double this_x_min = at_cds_[i].x;
          double this_x_max = at_cds_[i].x;
          double this_y = at_cds_[i].y - atsym_height/2;
          if( W == atom_syms_[i].second ) {
            this_x_min -= atsym_width;
          } else if( E == atom_syms_[i].second ) {
            this_x_max += atsym_width;
          } else {
            this_x_max += atsym_width/2;
            this_x_min -= atsym_width/2;
          }
          x_max = std::max( x_max , this_x_max );
          x_min_ = std::min( x_min_ , this_x_min );
          y_max = std::max( y_max , this_y );
        }
      }
      double old_scale = scale_;
      x_range_ = x_max - x_min_;
      y_range_ = y_max - y_min_;
      scale_ = std::min( double( width_ ) / x_range_ , double( height_ ) / y_range_ );
      if( fabs( scale_ - old_scale ) < 0.1 ) {
        break;
      }
    }

    // put a 5% buffer round the drawing and calculate a final scale
    x_min_ -= 0.05 * x_range_;
    x_range_ *= 1.1;
    y_min_ -= 0.05 * y_range_;
    y_range_ *= 1.1;

    scale_ = std::min( double( width_ ) / x_range_ , double( height_ ) / y_range_ );

    double x_mid = x_min_ + 0.5 * x_range_;
    double y_mid = y_min_ + 0.5 * y_range_;
    Point2D mid = getDrawCoords( Point2D( x_mid , y_mid ) );
    x_trans_ = ( width_ / 2 - mid.x ) / scale_;
    y_trans_ = ( height_ / 2 - mid.y ) / scale_;
  }

  // ****************************************************************************
  // establishes whether to put string draw mode into super- or sub-script
  // mode based on contents of instring from i onwards. Increments i appropriately
  // and returns true or false depending on whether it did something or not.
  bool MolDraw2D::setStringDrawMode( const string &instring , int &draw_mode ,
                                     int &i ) const {

    string bit1 = instring.substr( i , 5 );
    string bit2 = instring.substr( i , 6 );

    // could be markup for super- or sub-script
    if( string( "<sub>" ) == bit1 ) {
      draw_mode = 2;
      i += 4;
      return true;
    } else if( string( "<sup>" ) == bit1 ) {
      draw_mode = 1;
      i += 4;
      return true;
    } else if( string( "</sub>") == bit2 ) {
      draw_mode = 0;
      i += 5;
      return true;
    } else if( string( "</sup>") == bit2 ) {
      draw_mode = 0;
      i += 5;
      return true;
    }

    return false;
  }

  // ****************************************************************************
  void MolDraw2D::drawLine( const Point2D &cds1 ,
                            const Point2D &cds2 ,
                            const DrawColour &col1 , const DrawColour &col2) {

    if( col1 == col2 ) {
      setColour( col1 );
      drawLine( cds1 , cds2 );
    } else {
      Point2D mid=(cds1+cds2);
      mid *= .5;

      setColour( col1 );
      drawLine( cds1 , mid );
      setColour( col2 );
      drawLine( mid , cds2 );
    }
  }

  // ****************************************************************************
  // draws the string centred on cds
  void MolDraw2D::drawString( const string &str ,
                              const Point2D &cds ) {

    double string_width , string_height;
    getStringSize( str , string_width , string_height );

    double draw_x = cds.x - string_width / 2.0;
    double draw_y = cds.y - string_height / 2.0;

    double full_font_size = fontSize();
    int draw_mode = 0; // 0 for normal, 1 for superscript, 2 for subscript
    string next_char( " " );

    for( int i = 0 , is = str.length() ; i < is ; ++i ) {

      // setStringDrawMode moves i along to the end of any <sub> or <sup>
      // markup
      if( '<' == str[i] && setStringDrawMode( str , draw_mode , i ) ) {
        continue;
      }

      char next_c = str[i];
      next_char[0] = next_c;
      double char_width , char_height;
      getStringSize( next_char , char_width , char_height );

      // these font sizes and positions work best for Qt, IMO. They may want
      // tweaking for a more general solution.
      if( 2 == draw_mode ) {
        // y goes from top to bottom, so add for a subscript!
        setFontSize( 0.75 * full_font_size );
        char_width *= 0.5;
        drawChar( next_c , getDrawCoords( Point2D( draw_x , draw_y + 0.5 * char_height ) ) );
        setFontSize( full_font_size );
      } else if( 1 == draw_mode ) {
        setFontSize( 0.75 * full_font_size );
        char_width *= 0.5;
        drawChar( next_c , getDrawCoords( Point2D( draw_x , draw_y - 0.25 * char_height ) ) );
        setFontSize( full_font_size );
      } else {
        drawChar( next_c , getDrawCoords( Point2D( draw_x , draw_y ) ) );
      }
      draw_x += char_width;
    }
  }

  // ****************************************************************************
  DrawColour MolDraw2D::getColour( int atom_idx ,
                                   const std::vector<int> *highlight_atoms ,
                                   const std::map<int,DrawColour> *highlight_map ) {

    PRECONDITION(atom_idx>=0,"bad atom_idx");
    PRECONDITION(atomic_nums_.size()>atom_idx,"bad atom_idx");
    DrawColour retval = getColourByAtomicNum( atomic_nums_[atom_idx] );

    // set contents of highlight_atoms to red
    if(!drawOptions().circleAtoms && !drawOptions().continuousHighlight){
      if( highlight_atoms && highlight_atoms->end() != find( highlight_atoms->begin() ,
          highlight_atoms->end() , atom_idx )  ) {
        retval = drawOptions().highlightColour;
      }
      // over-ride with explicit colour from highlight_map if there is one
      if(highlight_map){
        map<int,DrawColour>::const_iterator p = highlight_map->find( atom_idx );
        if( p != highlight_map->end() ) {
          retval = p->second;
        }
      }
    }
    return retval;
  }

  // ****************************************************************************
  DrawColour MolDraw2D::getColourByAtomicNum( int atomic_num ) {

    // RGB values taken from Qt's QColor. The seem to work pretty well on my
    // machine. Using them as fractions of 255, as that's the way Cairo does it.
    DrawColour res(0.,1.,1.); // default to cyan

    switch( atomic_num ) {
    case 0 : res=DrawColour(0.5,0.5,0.5); break;
    case 1 : res=DrawColour(0.55,0.55,0.55); break;
    case 6 : res=DrawColour(0.0,0.0,0.0); break;
    case 7 : res=DrawColour(0.0,0.0,1.0); break;
    case 8 : res=DrawColour(1.0,0.0,0.0); break;
    case 9 : res=DrawColour(0.2,0.8,0.8); break;
    case 15 : res=DrawColour(1.0,0.5,0.0); break;
    case 16 : res=DrawColour(0.8,0.8,0.0); break;
    case 17 : res=DrawColour(0.0,0.802,0.0); break;
    case 35 : res=DrawColour(0.5,0.3,0.1); break;
    case 53 : res=DrawColour(0.63,0.12,0.94); break;
    default : break;
    }

    return res;
  }

  // ****************************************************************************
  void MolDraw2D::extractAtomCoords( const ROMol &mol,int confId ) {

    at_cds_.clear();
    atomic_nums_.clear();
    const RDGeom::POINT3D_VECT &locs = mol.getConformer( confId ).getPositions();
    ROMol::VERTEX_ITER this_at , end_at;
    boost::tie( this_at , end_at ) = mol.getVertices();
    while( this_at != end_at ) {
      int this_idx = mol[*this_at]->getIdx();
      at_cds_.push_back( Point2D( locs[this_idx].x , locs[this_idx].y ) );
      ++this_at;
    }
  }

  // ****************************************************************************
  void MolDraw2D::extractAtomSymbols( const ROMol &mol ) {

    ROMol::VERTEX_ITER atom , end_atom;
    boost::tie( atom , end_atom ) = mol.getVertices();
    while( atom != end_atom ) {
      ROMol::OEDGE_ITER nbr , end_nbrs;
      const Atom *at1 = mol[*atom].get();
      boost::tie( nbr , end_nbrs ) = mol.getAtomBonds( at1 );
      Point2D &at1_cds = at_cds_[at1->getIdx()];
      Point2D nbr_sum( 0.0 , 0.0 );
      while( nbr != end_nbrs ) {
        const BOND_SPTR bond = mol[*nbr];
        ++nbr;
        Point2D &at2_cds = at_cds_[bond->getOtherAtomIdx( at1->getIdx() )];
        nbr_sum += at2_cds-at1_cds;
      }
      atom_syms_.push_back( getAtomSymbolAndOrientation( *at1 , nbr_sum ) );
      atomic_nums_.push_back( at1->getAtomicNum() );
      ++atom;
    }
  }

  // ****************************************************************************
  void MolDraw2D::drawBond( const ROMol &mol , const BOND_SPTR &bond ,
                            int at1_idx , int at2_idx ,
                            const vector<int> *highlight_atoms ,
                            const map<int,DrawColour> *highlight_atom_map ,
                            const vector<int> *highlight_bonds ,
                            const map<int,DrawColour> *highlight_bond_map
                            ) {
    static const DashPattern noDash;
    static const DashPattern dots=assign::list_of(2)(6);
    static const DashPattern dashes=assign::list_of(6)(6);

    const Atom *at1 = mol.getAtomWithIdx( at1_idx );
    const Atom *at2 = mol.getAtomWithIdx( at2_idx );
    const double double_bond_offset = 0.1;

    Point2D at1_cds = at_cds_[at1_idx];
    Point2D at2_cds = at_cds_[at2_idx];

    adjustBondEndForLabel( at1_idx , at2_cds , at1_cds );
    adjustBondEndForLabel( at2_idx , at1_cds , at2_cds );

    bool highlight_bond=false;
    if(highlight_bonds &&
       std::find(highlight_bonds->begin(),highlight_bonds->end(),bond->getIdx()) != highlight_bonds->end()){
      highlight_bond=true;
    }

    DrawColour col1,col2;
    int orig_lw=lineWidth();
    if( !highlight_bond ){
      col1 = getColour( at1_idx );
      col2 = getColour( at2_idx );
    } else {
      if(highlight_bond_map &&
         highlight_bond_map->find(bond->getIdx())!=highlight_bond_map->end()){
        col1 = col2 = highlight_bond_map->find(bond->getIdx())->second;
      } else {
        col1 = col2 = drawOptions().highlightColour;
      }
      if( drawOptions().continuousHighlight ){
        setLineWidth(orig_lw*8);
      } else {
        setLineWidth(orig_lw*2);
      }
    }


    Bond::BondType bt=bond->getBondType();
    bool isComplex=false;
    if(bond->hasQuery()){
      std::string descr=bond->getQuery()->getDescription();
      if( bond->getQuery()->getNegation() || descr!="BondOrder"){
        isComplex=true;
      }
      if(isComplex){
        setDash(dots);
        drawLine( at1_cds , at2_cds , col1 , col2 );
        setDash(noDash);
      } else {
        bt = static_cast<Bond::BondType>(static_cast<BOND_EQUALS_QUERY *>(bond->getQuery())->getVal());
      }
    }
      
             
    if(!isComplex){
      // it's a double bond and one end is 1-connected, do two lines parallel
      // to the atom-atom line.
      if( ( bt == Bond::DOUBLE ) &&
          ( 1 == at1->getDegree() || 1 == at2->getDegree() ) ) {
        Point2D perp = calcPerpendicular( at1_cds , at2_cds ) * double_bond_offset ;
        drawLine(at1_cds + perp, at2_cds + perp, col1,col2);
        drawLine(at1_cds - perp, at2_cds - perp, col1,col2);
        if( bt == Bond::TRIPLE ) {
          drawLine( at1_cds , at2_cds , col1 , col2 );
        }
      } else if( Bond::SINGLE == bt &&
                 ( Bond::BEGINWEDGE == bond->getBondDir() || Bond::BEGINDASH == bond->getBondDir() ) ) {
        if( at1->getChiralTag() != Atom::CHI_TETRAHEDRAL_CW &&
            at1->getChiralTag() != Atom::CHI_TETRAHEDRAL_CCW ) {
          swap( at1_cds , at2_cds );
          swap( col1 , col2 );
        }
        if( Bond::BEGINWEDGE == bond->getBondDir() ) {
          drawWedgedBond( at1_cds , at2_cds , false , col1 , col2 );
        } else {
          drawWedgedBond( at1_cds , at2_cds , true , col1 , col2 );
        }
      } else {
        // in all other cases, we will definitely want to draw a line between the
        // two atoms
        drawLine( at1_cds , at2_cds , col1 , col2 );
        if( Bond::TRIPLE == bt ) {
          // 2 further lines, a bit shorter and offset on the perpendicular
          double dbo = 2.0 * double_bond_offset;
          Point2D perp = calcPerpendicular( at1_cds , at2_cds );
          double end1_trunc = 1 == at1->getDegree() ? 0.0 : 0.1;
          double end2_trunc = 1 == at2->getDegree() ? 0.0 : 0.1;
          Point2D bv=at1_cds - at2_cds;
          Point2D p1=at1_cds - (bv*end1_trunc) + perp*dbo;
          Point2D p2=at2_cds + (bv*end2_trunc) + perp*dbo;
          drawLine( p1 , p2 , col1 , col2 );
          p1=at1_cds - (bv*end1_trunc) - perp*dbo;
          p2=at2_cds + (bv*end2_trunc) - perp*dbo;
          drawLine( p1 , p2 , col1 , col2 );
        }
        // all we have left now are double bonds in a ring or not in a ring
        // and multiply connected
        else if( Bond::DOUBLE == bt || Bond::AROMATIC == bt) {
          Point2D perp;
          if( mol.getRingInfo()->numBondRings( bond->getIdx() ) ) {
            // in a ring, we need to draw the bond inside the ring
            perp = bondInsideRing( mol , bond , at1_cds , at2_cds );
          } else {
            perp = bondInsideDoubleBond( mol , bond );
          }
          double dbo = 2.0 * double_bond_offset;
          Point2D bv=at1_cds - at2_cds;
          Point2D p1 = at1_cds - bv * 0.1 + perp * dbo;
          Point2D p2 = at2_cds + bv * 0.1 + perp * dbo;
          if(bt==Bond::AROMATIC) setDash(dashes);
          drawLine( p1, p2, col1 , col2 );
          if(bt==Bond::AROMATIC) setDash(noDash);
        }
      } 
    }
    if(highlight_bond){
      setLineWidth(orig_lw);
    }

  }

  // ****************************************************************************
  void MolDraw2D::drawWedgedBond( const Point2D &cds1 ,
                                  const Point2D &cds2 ,
                                  bool draw_dashed , const DrawColour &col1 ,
                                  const DrawColour &col2) {
    Point2D perp = calcPerpendicular( cds1 , cds2 );
    Point2D disp = perp*0.1;
    Point2D end1 = cds2 + disp;
    Point2D end2 = cds2 - disp;

    setColour( col1 );
    if( draw_dashed ) {
      Point2D e1=end1-cds1;
      Point2D e2=end2-cds1;
      for( int i = 1 ; i < 11 ; ++i ) {
        if( 6 == i ) {
          setColour( col2 );
        }
        Point2D e11 = cds1 + e1*0.1*i;
        Point2D e22 = cds1 + e2*0.1*i;
        drawLine( e11 , e22 );
      }
    } else {
      if( col1 == col2 ) {
        drawTriangle( cds1 , end1 , end2 );
      } else {
        Point2D e1 = end1 - cds1;
        Point2D e2 = end2 - cds1;
        Point2D mid1 = cds1 + e1*0.5;
        Point2D mid2 = cds1 + e2*0.5;
        drawTriangle( cds1 , mid1 , mid2 );
        setColour( col2 );
        drawTriangle( mid1 , end2 , end1 );
        drawTriangle( mid1 , mid2 , end2 );
      }
    }
  }

  // ****************************************************************************
  void MolDraw2D::drawAtomLabel( int atom_num ,
                                 const std::vector<int> *highlight_atoms ,
                                 const std::map<int,DrawColour> *highlight_map ) {

    setColour( getColour( atom_num , highlight_atoms , highlight_map ) );
    drawString( atom_syms_[atom_num].first , at_cds_[atom_num] );
  }

  // ****************************************************************************
  // calculate normalised perpendicular to vector between two coords
  Point2D MolDraw2D::calcPerpendicular( const Point2D &cds1 ,
                                                  const Point2D &cds2 ) {

    double bv[2] = { cds1.x - cds2.x , cds1.y - cds2.y };
    double perp[2] = { -bv[1] , bv[0] };
    double perp_len = sqrt( perp[0] * perp[0] + perp[1] * perp[1] );
    perp[0] /= perp_len; perp[1] /= perp_len;

    return Point2D( perp[0] , perp[1] );
  }

  // ****************************************************************************
  // cds1 and cds2 are 2 atoms in a ring.  Returns the perpendicular pointing into
  // the ring
  Point2D MolDraw2D::bondInsideRing( const ROMol &mol , const BOND_SPTR &bond ,
                                               const Point2D &cds1 ,
                                               const Point2D &cds2 ) {

    Atom *bgn_atom = bond->getBeginAtom();
    ROMol::OEDGE_ITER nbr2 , end_nbrs2;
    boost::tie( nbr2 , end_nbrs2 ) = mol.getAtomBonds( bgn_atom );
    while( nbr2 != end_nbrs2 ) {
      const BOND_SPTR bond2 = mol[*nbr2];
      ++nbr2;
      if( bond2->getIdx() == bond->getIdx() ||
          !mol.getRingInfo()->numBondRings( bond2->getIdx() ) ) {
        continue;
      }
      bool same_ring = false;
      BOOST_FOREACH( const INT_VECT &ring , mol.getRingInfo()->bondRings() ) {
        if( find( ring.begin() , ring.end() , bond->getIdx() ) != ring.end() &&
            find( ring.begin() , ring.end() , bond2->getIdx() ) != ring.end() ) {
          same_ring = true;
          break;
        }
      }
      if( same_ring ) {
        // bond and bond2 are in the same ring, so use their vectors to define
        // the sign of the perpendicular.
        int atom3 = bond2->getOtherAtomIdx( bond->getBeginAtomIdx() );
        return calcInnerPerpendicular( cds1 , cds2 , at_cds_[atom3] );
      }
    }

    return calcPerpendicular( cds1 , cds2 );
  }

  // ****************************************************************************
  // cds1 and cds2 are 2 atoms in a chain double bond.  Returns the perpendicular
  // pointing into the inside of the bond
  Point2D MolDraw2D::bondInsideDoubleBond( const ROMol &mol , const BOND_SPTR &bond ) {

    // a chain double bond, were it looks nicer IMO if the 2nd line is inside
    // the angle of outgoing bond. Unless it's an allene, where nothing
    // looks great.
    const Atom *at1 = bond->getBeginAtom();
    const Atom *at2 = bond->getEndAtom();
    const Atom *bond_atom , *end_atom;
    if( at1->getDegree() > 1 ) {
      bond_atom = at1;
      end_atom = at2;
    } else {
      bond_atom = at2;
      end_atom = at1;
    }
    int at3 = -1; // to stop the compiler whinging.
    ROMol::OEDGE_ITER nbr2 , end_nbrs2;
    boost::tie( nbr2 , end_nbrs2 ) = mol.getAtomBonds( bond_atom );
    while( nbr2 != end_nbrs2 ) {
      const BOND_SPTR bond2 = mol[*nbr2];
      ++nbr2;
      if( bond != bond2 ) {
        at3 = bond2->getOtherAtomIdx( bond_atom->getIdx() );
        break;
      }
    }

    return calcInnerPerpendicular( at_cds_[end_atom->getIdx()] ,
                                   at_cds_[bond_atom->getIdx()] , at_cds_[at3] );
  }

  // ****************************************************************************
  // calculate normalised perpendicular to vector between two coords, such that
  // it's inside the angle made between (1 and 2) and (2 and 3).
  Point2D MolDraw2D::calcInnerPerpendicular( const Point2D &cds1 ,
                                                       const Point2D &cds2 ,
                                                       const Point2D &cds3 ) {

    Point2D perp = calcPerpendicular( cds1 , cds2 );
    double v1[2] = { cds1.x - cds2.x , cds1.y - cds2.y };
    double v2[2] = { cds2.x - cds3.x , cds2.y - cds3.y };
    double obv[2] = { v1[0] - v2[0] , v1[1] - v2[1] };

    // if dot product of centre_dir and perp < 0.0, they're pointing in opposite
    // directions, so reverse perp
    if( obv[0] * perp.x + obv[1] * perp.y < 0.0 ) {
      perp.x *= -1.0;
      perp.y *= - 1.0;
    }

    return perp;
  }

  // ****************************************************************************
  // take the coords for atnum, with neighbour nbr_cds, and move cds out to accommodate
  // the label associated with it.
  void MolDraw2D::adjustBondEndForLabel( int atnum , const Point2D &nbr_cds ,
                                         Point2D &cds ) const {

    if( atom_syms_[atnum].first.empty() ) {
      return;
    }

    double label_width , label_height;
    getStringSize( atom_syms_[atnum].first , label_width , label_height );

    double lw2 = label_width / 2.0;
    double lh2 = label_height / 2.0;

    double x_offset = 0.0 , y_offset = 0.0;
    if( fabs( nbr_cds.y - cds.y ) < 1.0e-5 ) {
      // if the bond is horizontal
      x_offset = lw2;
    } else {
      x_offset = fabs( lh2 * ( nbr_cds.x - cds.x ) / ( nbr_cds.y - cds.y ) );
      if( x_offset >= lw2 ) {
        x_offset = lw2;
      }
    }
    if( nbr_cds.x < cds.x ) {
      x_offset *= -1.0;
    }

    if( fabs( nbr_cds.x - cds.x ) < 1.0e-5 ) {
      // if the bond is vertical
      y_offset = lh2;
    } else {
      y_offset = fabs( lw2 * ( cds.y - nbr_cds.y ) / ( nbr_cds.x - cds.x ) );
      if( y_offset >= lh2 ) {
        y_offset = lh2;
      }
    }
    if( nbr_cds.y < cds.y ) {
      y_offset *= -1.0;
    }

    cds.x += x_offset;
    cds.y += y_offset;
  }

  // ****************************************************************************
  // adds XML-like annotation for super- and sub-script, in the same manner
  // as MolDrawing.py. My first thought was for a LaTeX-like system, obviously...
  pair<string,MolDraw2D::OrientType> MolDraw2D::getAtomSymbolAndOrientation( const Atom &atom ,
                                                                             const Point2D &nbr_sum ) {

    string symbol( "" );

    // -----------------------------------
    // start with the symbol
    if(drawOptions().atomLabels.find(atom.getIdx()) != drawOptions().atomLabels.end() ){
      // specified labels are trump: no matter what else happens we will show them.
      symbol = drawOptions().atomLabels.find(atom.getIdx())->second;
    } else if(drawOptions().dummiesAreAttachments && atom.getAtomicNum()==0 && atom.getDegree()==1) {
      symbol="";
    } else if(isComplexQuery(&atom)) {
      symbol="?";
    } else {
      std::vector<std::string> preText,postText;
      if( 0 != atom.getIsotope() ) {
        preText.push_back( std::string("<sup>") + 
                           lexical_cast<string>( atom.getIsotope() ) + 
                           std::string("</sup>") );
      }

      if( atom.hasProp( "molAtomMapNumber" ) ) {
        string map_num="";
        atom.getProp( "molAtomMapNumber" , map_num );
        postText.push_back(std::string(":") + map_num);
      }

      std::string h="";
      int num_h=atom.getAtomicNum()==6 ? 0 : atom.getTotalNumHs(); // FIX: still not quite right
      if( num_h > 0 && !atom.hasQuery() ) {
        h = "H";
        if( num_h > 1 ) {
          // put the number as a subscript
          h += string( "<sub>" ) + lexical_cast<string>( num_h ) + string( "</sub>" );
        }
        postText.push_back(h);
      }

      if( 0 != atom.getFormalCharge() ) {
        int ichg = atom.getFormalCharge();
        string sgn = ichg > 0 ? string( "+" ) : string( "-" );
        ichg = abs( ichg );
        if( ichg > 1 ) {
          sgn += lexical_cast<string>( ichg );
        }
        // put the charge as a superscript
        postText.push_back(string( "<sup>" ) + sgn + string( "</sup>" ));
      }

      symbol="";
      BOOST_FOREACH(const std::string &se,preText){
        symbol += se;
      }
      if(atom.getAtomicNum()!=6 || preText.size() || postText.size() ){
        symbol += atom.getSymbol();
      }
      BOOST_FOREACH(const std::string &se,postText){
        symbol += se;
      }

    }

    // -----------------------------------
    // now consider the orientation
    OrientType orient = C;
    if( 1 == atom.getDegree() ) {
      double islope = 0.0;
      if( fabs( nbr_sum.y ) > 1.0 ) {
        islope = nbr_sum.x / fabs( nbr_sum.y );
      } else {
        islope = nbr_sum.x;
      }
      if( fabs( islope ) > 0.85 ) {
        if( islope > 0.0 ) {
          orient = W;
        } else {
          orient = E;
        }
      } else {
        if( nbr_sum.y > 0.0 ) {
          orient = N;
        } else {
          orient = S;
        }
      }
    }
    return std::make_pair( symbol , orient );
  }

  void MolDraw2D::drawTriangle( const Point2D &cds1 ,
                                const Point2D &cds2 ,
                                const Point2D &cds3 ) {
    std::vector< Point2D > pts(3);
    pts[0]=cds1;
    pts[1]=cds2;
    pts[2]=cds3;
    drawPolygon(pts);
  };

  // ****************************************************************************
  void MolDraw2D::drawEllipse(const Point2D &cds1 ,
                              const Point2D &cds2){
    std::vector< Point2D > pts;
    MolDraw2D_detail::arcPoints(cds1,cds2,pts,0,360);
    drawPolygon(pts);
  }
  // ****************************************************************************
  void MolDraw2D::drawRect(const Point2D &cds1 ,
                           const Point2D &cds2){
    std::vector< Point2D > pts(4);
    pts[0]=cds1;
    pts[1]=Point2D(cds1.x,cds2.y);
    pts[2]=cds2;
    pts[3]=Point2D(cds2.x,cds1.y);
    drawPolygon(pts);
  }

  // ****************************************************************************
  //  we draw the line at cds2, perpendicular to the line cds1-cds2
  void MolDraw2D::drawAttachmentLine( const Point2D &cds1 ,
                                      const Point2D &cds2 ,   
                                      const DrawColour &col,
                                      double len,
                                      unsigned int nSegments ) {
    if(nSegments%2) ++nSegments; // we're going to assume an even number of segments
    Point2D perp = calcPerpendicular( cds1 , cds2 );
    Point2D p1 = Point2D(cds2.x-perp.x*len/2,
                                     cds2.y-perp.y*len/2);
    Point2D p2 = Point2D(cds2.x+perp.x*len/2,
                                     cds2.y+perp.y*len/2);
    setColour( col );
    drawLine(p1,p2);
  }




} // EO namespace RDKit
