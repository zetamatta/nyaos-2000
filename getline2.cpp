#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef __DMC__
#  include <dos.h>
#endif

#include <string.h>
#include "nndir.h"
#include "getline.h"


extern int read_shortcut(const char *src,char *buffer,int size);

int NnPair::compare( const NnSortable &x ) const
{
    return first()->compare( *((const NnPair&)x).first() );
}

/* ���ϐ��̕⊮���s��.
 *    startpos �c ���ϐ��\���J�n�ʒu('%'�̈ʒu)
 *    endpos   �c ������(�Ȃ����� ��)
 *    wildcard �c �W�J�O������S��
 *    array    �c �⊮��������z��
 * return
 *     >=0 �⊮��␔.
 *     < 0 ���ϐ��̕⊮�̕K�v�͂Ȃ��A�����Ƃ��Ă͓W�J��������.
 */
static int expand_environemnt_variable(
	int startpos , 
	int endpos ,
	const char *terminate_string ,
	NnString &wildcard ,
	NnVector &array )
{
    NnString name;
    if( endpos >= 0 ){
	/* ���ϐ������������Ă���̂ŁA�W�J����̂� */
	name.assign( wildcard.chars()+startpos+1 , endpos-startpos-1 );
	const char *value=getEnv( name.chars() , NULL );
	if( value != NULL ){
	    NnString tmp( wildcard.chars() , startpos );
	    tmp << value << (wildcard.chars()+endpos+1) ;
	    wildcard = tmp;
	}
	return -1;
    }else{
	/* ���ϐ������������Ă��Ȃ� �� ���ϐ�����⊮�̑ΏۂƂ��� */
	name = wildcard.chars()+startpos+1 ;
	name.upcase();
	for( char **p=environ ; *p != NULL ; ++p ){
	    if( strnicmp( *p , name.chars() , name.length()) == 0 ){
		NnString *str = new NnString( wildcard.chars() , startpos+1 );
		int eqlpos=NnString::findOf(*p,"=");
		if( eqlpos >= 0 ){
		    str->insertAt( str->length() , *p , eqlpos );
		}else{
		    *str << *p;
		}
		*str << terminate_string;

		array.append( new NnPair(str) );
	    }
	}
	return array.size();
    }
}



/* �⊮��⃊�X�g���쐬����.
 *      region  �p�X���܂ޔ͈�(���p���܂�)
 *      array   ��⃊�X�g�������
 * return
 *      ��␔
 */
int GetLine::makeCompletionList( const NnString &region, NnVector &array )
{
    int i;
    NnString path;

    /* ���p����S�č폜���� */
    path = region;
    path.dequote();

    int lastroot=NnDir::lastRoot( path.chars() );
    char rootchar=( lastroot != -1 && path.at(lastroot)=='/' ? '/' : '\\' );

    /* ���C���h�J�[�h������̍쐬 */
    NnString wildcard;
    NnDir::f2b( path.chars() , wildcard );
    if( wildcard.at(0) == '~' && 
	(isalnum(wildcard.at(1) & 255) || wildcard.at(1)=='_' ) && 
	lastroot == -1 )
    {
	/* ~ �Ŏn�܂�A���[�g���܂܂�Ă��炸�A�񕶎��ڂɉp����������ꍇ��
	 * �s���S�ȓ���t�H���_�[���������Ă���Ƃ݂Ȃ��B
	 */
	wildcard.shift();
	for( NnHash::Each itr(NnDir::specialFolder) ; *itr != NULL ; ++itr ){
	    if( itr->key().startsWith(wildcard) ){
		NnString *str=new NnString();
		*str << '~' << itr->key() << rootchar ;
		array.append( new NnPair(str) );
	    }
	}
	return array.size();
    }
    /* �p�X�Ɋ��ϐ����܂܂�Ă�����ϊ����Ă��� */
    /* %�c% �`���̊��ϐ��\�L */
    if( (i=wildcard.findOf("%")) >= 0 ){
	int j=wildcard.findOf("%",i+1);
	int result = expand_environemnt_variable(i,j,"%",wildcard,array);
	if( result >= 0 )
	    return result;
    }
    /* $�c �`���̊��ϐ��\�L */
    if( (i=wildcard.findOf("$")) >= 0 ){
	int result;
	if( wildcard.at(i+1) == '{' ){
	    /* ${�c} �`�� */
	    result = expand_environemnt_variable(
		    i+1, wildcard.findOf("}",i+2),
		    "}", wildcard,array );
	}else{
	    /* $�c �`�� */
	    result = expand_environemnt_variable(
		    i, wildcard.findOf(" \t",i+1),
		    "",wildcard,array );
	}
	if( result >= 0 )
	    return result;
    }

    int has_wildcard=0;
    if( wildcard.findOf("*?") >= 0 )
	has_wildcard = 1;
    else
	wildcard += '*';

    /* �f�B���N�g�������𒊏o���� */
    NnString basename;
    if( lastroot >= 0 )
        basename.assign( path.chars() , lastroot+1 );
  
    /* �����쐬���� */
    i=0;
    for( NnDir dir(wildcard) ; dir.more() ; dir.next() ){
        // �u.�v�u..�v��r������.
        if(    dir->at(0) == '.' 
            && (    dir->at(1) == '\0'
                || (dir->at(1) == '.' && dir->at(2)=='\0' ) )){
            continue;
        }
        // �����O�t�@�C�����̓}�b�`���Ȃ����A�V���[�g�t�@�C�������}�b�`
        // ���Ă��܂��P�[�X��r������.
	// (�������A���C���h�J�[�h���u�����I�Ɂv���p���Ă���ꍇ�͏���)
        if( strnicmp( dir.name() , path.chars() +(lastroot+1)
                    , path.length()-(lastroot+1) ) != 0 
	    && ! has_wildcard )
            continue;

        NnStringIC *name=new NnStringIC(basename);
        if( name == NULL )
            break;

        *name += dir.name();
        if( dir.isDir() )
            *name += rootchar;
        if( array.append( 
		new NnPair(name,new NnString(name->chars()+basename.length() )) 
	    ) != 0 )
	{
            delete name;
            break;
        }
        ++i;
    }
    return i;
}
/* �R�}���h���⊮��⃊�X�g���쐬����.
 * (�{�N���X�ł́AmakeCompletionList �Ɠ����B�����\�b�h�͔h���N���X��
 *  �I�[�o�[���C�h�����)
 *      region - �Ώۂ̕�����
 *      array - �⊮��₪����x�N�^�[
 * return
 *      ��␔
 */
int GetLine::makeTopCompletionList( const NnString &region , NnVector &array )
{
    return makeCompletionList( region , array );
}

/* ������ s1 �� ������ s2 �Ƃ̋��ʕ����̒����𓾂� */
static int sameLength(const char *s1 , const char *s2)
{
    int len=0;
    while( *s1 != '\0' && *s2 != '\0' ){
        if( isKanji(*s1 & 255) ){
            if( ! isKanji(*s2 & 255) )
                break;
            if( s1[0] != s2[0] || s1[1] != s2[1] )
                break;
            s1 += 2;
            s2 += 2;
            len += 2;
        }else{
            if( isKanji(*s2 & 255) )
                break;
            if( toupper(*s1 & 255) != toupper(*s2 & 255) )
                break;
            ++s1;
            ++s2;
            ++len;
        }
    }
    return len;
}

struct Completion {
    NnString word ;
    int at , size , n ;
    NnVector list ;

    NnString &name(int n){
	return *(NnString*)((NnPair*)list.at(n))->second_or_first();
    }
    NnString &path(int n){ return *(NnString*)((NnPair*)list.at(n))->first();  }
    NnString &operator[](int n){ return path(n); }
    int maxlen();
};

/* ��⃊�X�g�̍ő�T�C�Y�𓾂� */
int Completion::maxlen()
{
    int max=0;
    for(int i=0;i<n;i++){
        int temp=this->name(i).length();
	if( temp > max )
            max = temp;
    }
    return max;
}


/* �J�[�\���ʒu�̕������ǂ݂Ƃ��āA����������.
 *      r - �⊮��
 * return
 *    >=0 : ��␔
 *    -1  : �P�ꂪ�Ȃ�.
 */
int GetLine::read_complete_list( Completion &r )
{
    r.word=current_word( r.at , r.size );
    if( r.word.empty()  && properties.get("nullcomplete") == NULL )
	return -1;

    // �⊮���X�g�쐬
    return r.n = ( r.at == 0 ? makeTopCompletionList( r.word , r.list ) 
                             : makeCompletionList   ( r.word , r.list ) );
}

/* �J�[�\���ʒu�`size�����O�܂ł̕����̒u�� & �ĕ\�����s���B
 *	size - �u�����T�C�Y
 *	match - �u��������
 * ����
 * �Esize < match.length() �ł���K�v������B
 * �E�J�[�\���� at+size �̈ʒu�ɂ��邱�Ƃ��K�v
 */
void GetLine::replace_repaint_here( int size , const NnString &match )
{
    int at=pos-size;
    buffer.replace( at, size , match.chars() );
    int oldpos=pos;
    pos += match.length() - size ;

    // �\���̍X�V.
    if( pos >= offset+width ){
        // �J�[�\���ʒu���E�C���h�E�O�ɂ���Ƃ�.
        putbs( oldpos-offset );
        offset = pos-width;
        puts_between(offset,pos);
    }else if( at < offset ){
        putbs( oldpos-offset );
        puts_between(offset,pos);
    }else{
        putbs( oldpos-at );
        puts_between(at,pos);
    }
    repaint_after( size <= match.length() ? 1 : size-match.length() );
}


/* �⊮�L�[ �Ή����\�b�h
 */
Status GetLine::complete(int)
{
    Completion comp;
    int i,n,hasSpace=0;

    if( (n=read_complete_list(comp)) <= 0 )
	return CONTINUE;
    
    if( comp.word.findOf(" \t\r\n!") != -1 )
	hasSpace = 1;

    // �⊮���̑������Ƃ肠�����o�b�t�@�փR�s�[.
    NnString match=comp[0];
    if( strchr( match.chars() , ' ' ) != NULL || comp.word.at(0) == '"' )
	hasSpace = 1;

    // ��₪��������ꍇ�́A
    // ��Ԗڈȍ~�̌��Ɣ�r���Ă䂫�A���ʕ����������c���Ă䂭.
    for( i=1; i < n ; ++i ){
	if( strchr( comp[i].chars() , ' ' ) != NULL )
	    hasSpace = 1;
	match.chop( sameLength( comp[i].chars() , match.chars() )); 
    }

    // (��{�I�ɂ��肦�Ȃ���)�A�⊮���̕����A����������Z���ꍇ��
    // ���������I������B
    if( match.length() <= comp.size && ! hasSpace )
	return CONTINUE;
    
    // ���C���h�J�[�h�⊮�����Ƃ��ɁA���̋��ʕ����S���Ȃ��ꍇ�A
    // ���p�������ɂȂ�̂�h���c
    if( match.length() == 0 )
	return CONTINUE;
    
    /* �V���[�g�J�b�g�W�J */
    if( n==1 && comp[0].iendsWith(".lnk") && properties.get("lnkexp") != NULL )
    {
	/* ��₪��ŁA���ꂪ�V���[�g�J�b�g�̏ꍇ */

	char buffer[ FILENAME_MAX ];
	if( read_shortcut( comp[0].chars() , buffer , sizeof(buffer)-1 ) == 0 ){
	    comp[0] = buffer;

	    NnDir stat( comp[0] );
	    if( stat.isDir() ){
		comp[0] << '\\';
	    }
	    match = comp[0];
	}
    }

    // �󔒂��܂ނƂ��A�擪�Ɂh������B
    if( hasSpace ){
	if( match.at(0)=='~' ){
	    for(int i=1; match.at(i) != '\0' ; ++i ){
		if( match.at(i) == '\\' || match.at(i) == '/' ){
		    match.insertAt(i+1,"\"");
		    break;
		}
	    }
	}else{
	    match.unshift( '"' );
	}
    }

    if( n==1 && match.length() > 0 ){ // ��₪������Ȃ��ꍇ�́c
	int tail=match.lastchar();
	// �󔒂�����ꍇ�́A���p�������B
	if( hasSpace )
	    match += '"';
	if( tail != '\\' && tail != '/' ){
	    // ��f�B���N�g���̏ꍇ�́A�����ɋ󔒂�����B
	    match += ' ';
	}
    }
    replace_repaint_here( comp.size , match );
    return CONTINUE;
}

void GetLine::listing_( Completion &comp )
{
    int maxlen = comp.maxlen();

    int nx= (80/(maxlen+1));
    if( nx <= 1 )
	nx = 1;
    int n=comp.n;
    int ny= (n+nx-1) / nx;
    NnString *prints=new NnString[ ny ];
    if( prints == NULL )
        return ;
    
    int i=0;
    for(int x=0;x<nx;x++){
        for(int y=0;y<ny;y++){
            if( i >= n )
               goto exit;
            prints[ y ] += comp.name(i);
            for( int j=comp.name(i).length() ; j<maxlen+1 ; j++ ){
                prints[ y ] << ' ';
            }
            i++;
        }
    }
  exit:
    putchr('\n');
    for(i=0;i<ny;i++){
        prints[i].trim();
        for(int j=0 ; j<prints[i].length() ; j++ )
            putchr( prints[i].at(j) );
        if( prints[i].length() > 0 )
            putchr('\n');
    }
#ifdef ARRAY_DELETE_NEED_SIZE
    delete [ny] prints;
#else
    delete [] prints;
#endif
    prompt();
    /*DEL*puts_between( offset , buffer.length() ); *2004.10.17*/
    puts_between( offset , pos );
    repaint_after();
}

Status GetLine::listing(int)
{
    // ��⊮������𔲂��o��.
    Completion comp;

    if( read_complete_list(comp) <= 0 )
	return CONTINUE;
  
    comp.list.sort();
    listing_( comp );
  
    return CONTINUE;
}

Status GetLine::complete_prev(int)
{ return complete_vzlike(-1); }

Status GetLine::complete_next(int)
{ return complete_vzlike(+1); }


enum{
    KEY_ALT_RETURN    = 0x11c ,
    KEY_ALT_BACKSPACE = 0x10e ,
    KEY_RIGHT         = 0x14d ,
    KEY_LEFT          = 0x14b ,
    KEY_DOWN          = 0x150 ,
    KEY_UP            = 0x148 ,
};
#define CTRL(x) ((x) & 0x1F)

Status GetLine::complete_vzlike(int direct)
{
    Completion comp;
    if( read_complete_list( comp ) <= 0 )
	return CONTINUE;
    
    int i=0,key=0,bs=0;
    if( comp.list.size() >= 2 ){
	comp.list.sort();

	if( direct < 0 )
	    i = comp.list.size()-1;
	int baseSize = comp.word.length();
	if( comp.word.at(0) == '"' )
	    baseSize--;
	
	if( pos - baseSize < offset ){
	    /* �P��̐擪����ʒ[���O�ɂȂ��Ă��܂��Ă��� */
	    bs = pos-offset ;
	    /* ��ʒ[�܂ŃJ�[�\����߂� */
	}else{
	    /* �P��̐擪�͉�ʏ�Ɍ����Ă��� */
	    bs = baseSize;
	    /* �P��[�܂ŃJ�[�\����߂� */
	}
	putbs( bs );
	pos      -= bs ;
	baseSize -= bs ;
	
	int sealSize=0;
	int maxlen1=comp.maxlen();
	for(;;){
	    NnString seal;
	    seal << (comp[i].chars() + baseSize );
	    for(int j=comp[i].length() ; j<maxlen1+1 ; ++j ){
		seal << ' ';
	    }
	    seal << '[';
	    seal.addValueOf( i+1 ) << '/' ;
	    seal.addValueOf( comp.list.size() ) << ']' ;
	    sealSize = printSeal( seal.chars() , sealSize );

	    key=getkey();
	    if(    which_command(key) == &GetLine::complete_next 
		|| which_command(key) == &GetLine::next 
		|| which_command(key) == &GetLine::vz_next ){
		/* ����� */
		if( ++i >= comp.list.size() ) 
		    i=0;
	    }else if(  which_command(key) == &GetLine::complete_prev 
		    || which_command(key) == &GetLine::previous
		    || which_command(key) == &GetLine::vz_previous ){
		/* �O��� */
		if( --i < 0 )
		    i=comp.list.size()-1;
	    }else if(   which_command(key) == &GetLine::erase_all 
		     || which_command(key) == &GetLine::abort ){
		/* �L�����Z�� */
		eraseSeal( sealSize );
		/* �J�[�\���ʒu��߂� */
		while( bs-- > 0 )
		   putchr( buffer[pos++] );
		return CONTINUE;
	    }else if(   which_command(key) == &GetLine::erase_or_listing
		     || which_command(key) == &GetLine::listing 
		     || which_command(key) == &GetLine::complete_or_listing ){
		/* ���X�g�o�� */
		listing_( comp );
		sealSize = 0;
		continue;
	    }else{ /* �m�� */
		break;
	    }
	}
	eraseSeal( sealSize );
    }

    NnString buf;
    if( comp[i].findOf(" \t\a\r\n\"") >= 0 ){
	buf << '"' << comp[i] << "\"";
    }else{
	buf << comp[i];
    }
    while( bs-- > 0 ){
	putchr(buffer[pos++]);
    }

    /* size������O�`�J�[�\���ʒu�܂ł�u�����Ă���� */
    replace_repaint_here( comp.size , buf );
    if( which_command(key) != &GetLine::enter )
	return interpret(key);

    return CONTINUE;
}

/* �^�u�L�[�̏����B�⊮���邩�A���X�g���o���B
 */
Status GetLine::complete_or_listing(int key)
{
    if( lastKey == key )
        return this->listing(key);
    else
        return this->complete(key);
}
