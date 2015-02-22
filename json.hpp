
#pragma once

#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <type_traits>
#include <initializer_list>
#include <ostream>
#include <iostream>

namespace json {

using std::map;
using std::vector;
using std::string;
using std::enable_if;
using std::initializer_list;
using std::is_same;
using std::is_convertible;
using std::is_integral;
using std::is_floating_point;

namespace {
    string json_escape( const string &str ) {
        string output;
        for( unsigned i = 0; i < str.length(); ++i )
            switch( str[i] ) {
                case '\"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '/' : output += "\\/";  break;
                case '\b': output += "\\b";  break;
                case '\f': output += "\\f";  break;
                case '\n': output += "\\n";  break;
                case '\r': output += "\\r";  break;
                case '\t': output += "\\t";  break;
                default  : output += str[i]; break;
            }
        return std::move( output );
    }
}

class JSON
{
    union BackingData {
        BackingData( double d ) : Float( d ){}
        BackingData( long   l ) : Int( l ){}
        BackingData( bool   b ) : Bool( b ){}
        BackingData( string s ) : String( new string( s ) ){}
        BackingData()           : Int( 0 ){}

        vector<JSON>       *List;
        map<string,JSON>   *Map;
        string             *String;
        double              Float;
        long                Int;
        bool                Bool;
    } Internal;

    public:
        enum class Class {
            Null,
            Object,
            Array,
            String,
            Floating,
            Integral,
            Boolean
        };

        JSON() : Internal(), Type( Class::Null ){}

        JSON( initializer_list<JSON> list ) 
            : JSON() 
        {
            SetType( Class::Object );
            for( auto i = list.begin(), e = list.end(); i != e; ++i, ++i )
                operator[]( i->ToString() ) = *std::next( i );
        }

        JSON( JSON&& other )
            : Internal( other.Internal )
            , Type( other.Type )
        { other.Type = Class::Null; other.Internal.Map = nullptr; }

        JSON& operator=( JSON&& other ) {
            Internal = other.Internal;
            Type = other.Type;
            other.Internal.Map = nullptr;
            other.Type = Class::Null;
            return *this;
        }

        JSON( const JSON &other ) {
            switch( other.Type ) {
            case Class::Object:
                Internal.Map = 
                    new map<string,JSON>( other.Internal.Map->begin(),
                                          other.Internal.Map->end() );
                break;
            case Class::Array:
                Internal.List = 
                    new vector<JSON>( other.Internal.List->begin(),
                                      other.Internal.List->end() );
                break;
            case Class::String:
                Internal.String = 
                    new string( *other.Internal.String );
                break;
            default:
                Internal = other.Internal;
            }
            Type = other.Type;
        }

        JSON& operator=( const JSON &other ) {
            switch( other.Type ) {
            case Class::Object:
                Internal.Map = 
                    new map<string,JSON>( other.Internal.Map->begin(),
                                          other.Internal.Map->end() );
                break;
            case Class::Array:
                Internal.List = 
                    new vector<JSON>( other.Internal.List->begin(),
                                      other.Internal.List->end() );
                break;
            case Class::String:
                Internal.String = 
                    new string( *other.Internal.String );
                break;
            default:
                Internal = other.Internal;
            }
            Type = other.Type;
            return *this;
        }

        ~JSON() {
            switch( Type ) {
            case Class::Array:
                delete Internal.List;
                break;
            case Class::Object:
                delete Internal.Map;
                break;
            case Class::String:
                delete Internal.String;
                break;
            default:;
            }
        }

        template <typename T>
        JSON( T b, typename enable_if<is_same<T,bool>::value>::type* = 0 ) : Internal( b ), Type( Class::Boolean ){};

        template <typename T>
        JSON( T i, typename enable_if<is_integral<T>::value && !is_same<T,bool>::value>::type* = 0 ) : Internal( (long)i ), Type( Class::Integral ){}

        template <typename T>
        JSON( T f, typename enable_if<is_floating_point<T>::value>::type* = 0 ) : Internal( (double)f ), Type( Class::Floating ){}

        template <typename T>
        JSON( T s, typename enable_if<is_convertible<T,string>::value>::type* = 0 ) : Internal( string( s ) ), Type( Class::String ){}

        JSON( std::nullptr_t ) : Internal(), Type( Class::Null ){}

        static JSON Make( Class type ) {
            JSON ret; ret.SetType( type );
            return ret;
        }

        static JSON Load( const string & );

        template <typename T>
        void append( T arg ) {
            SetType( Class::Array ); Internal.List->emplace_back( arg );
        }

        template <typename T, typename... U>
        void append( T arg, U... args ) {
            append( arg ); append( args... );
        }

        template <typename T>
            typename enable_if<is_same<T,bool>::value, JSON&>::type operator=( T b ) {
                SetType( Class::Boolean ); Internal.Bool = b; return *this;
            }

        template <typename T>
            typename enable_if<is_integral<T>::value && !is_same<T,bool>::value, JSON&>::type operator=( T i ) {
                SetType( Class::Integral ); Internal.Int = i; return *this;
            }

        template <typename T>
            typename enable_if<is_floating_point<T>::value, JSON&>::type operator=( T f ) {
                SetType( Class::Floating ); Internal.Float = f; return *this;
            }

        template <typename T>
            typename enable_if<is_convertible<T,string>::value, JSON&>::type operator=( T s ) {
                SetType( Class::String ); *Internal.String = string( s ); return *this;
            }

        JSON& operator[]( const string &key ) {
            SetType( Class::Object ); return Internal.Map->operator[]( key );
        }

        JSON& operator[]( unsigned index ) {
            SetType( Class::Array );
            if( index >= Internal.List->size() ) Internal.List->resize( index + 1 );
            return Internal.List->operator[]( index );
        }

        Class JSONType() { return Type; }

        /// Functions for getting primitives from the JSON object.
        bool IsNull(){ return Type == Class::Null; }

        string ToString() const { bool b; return std::move( ToString( b ) ); }
        string ToString( bool &ok ) const {
            ok = (Type == Class::String);
            return ok ? std::move( json_escape( *Internal.String ) ): string("");
        }

        double ToFloat() const { bool b; return ToFloat( b ); }
        double ToFloat( bool &ok ) const {
            ok = (Type == Class::Floating);
            return ok ? Internal.Float : 0.0;
        }

        long ToInt() const { bool b; return ToInt( b ); }
        long ToInt( bool &ok ) const {
            ok = (Type == Class::Integral);
            return ok ? Internal.Int : 0;
        }

        bool ToBool() const { bool b; return ToBool( b ); }
        bool ToBool( bool &ok ) const {
            ok = (Type == Class::Boolean);
            return ok ? Internal.Bool : false;
        }

        string dump( int depth = 1, string tab = "  ") const {
            string pad = "";
            for( int i = 0; i < depth; ++i, pad += tab );

            switch( Type ) {
                case Class::Null:
                    return "null";
                case Class::Object: {
                    string s = "{\n";
                    bool skip = true;
                    for( auto &p : *Internal.Map ) {
                        if( !skip ) s += ",\n";
                        s += ( pad + "\"" + p.first + "\" : " + p.second.dump( depth + 1, tab ) );
                        skip = false;
                    }
                    s += ( "\n" + pad.erase( 0, 2 ) + "}" ) ;
                    return s;
                }
                case Class::Array: {
                    string s = "[";
                    bool skip = true;
                    for( auto &p : *Internal.List ) {
                        if( !skip ) s += ", ";
                        s += p.dump( depth + 1, tab );
                        skip = false;
                    }
                    s += "]";
                    return s;
                }
                case Class::String:
                    return "\"" + json_escape( *Internal.String ) + "\"";
                case Class::Floating:
                    return std::to_string( Internal.Float );
                case Class::Integral:
                    return std::to_string( Internal.Int );
                case Class::Boolean:
                    return Internal.Bool ? "true" : "false";
                default:
                    return "";
            }
            return "";
        }

        friend std::ostream& operator<<( std::ostream&, const JSON & );

    private:
        void SetType( Class type ) {
            if( type == Type )
                return;

            /// Clear out any objects that are allocated 
            /// if we're removing an object/array.
            switch( Type ) {
            case Class::Object:
                delete Internal.Map;
                break;
            case Class::Array:
                delete Internal.List;
                break;
            case Class::String:
                delete Internal.String;
                break;
            default:;
            }

            /// Setup union as needed.
            switch( type ) {
            case Class::Null:
                Internal.Map = nullptr;
                break;
            case Class::Object:
                Internal.Map = new map<string,JSON>();
                break;
            case Class::Array:
                Internal.List = new vector<JSON>();
                break;
            case Class::String:
                Internal.String = new string();
                break;
            case Class::Floating:
                Internal.Float = 0.0;
                break;
            case Class::Integral:
                Internal.Int = 0;
                break;
            case Class::Boolean:
                Internal.Bool = false;
                break;
            }

            Type = type;
        }

    private:

        Class Type = Class::Null;
};

JSON Array() {
    return std::move( JSON::Make( JSON::Class::Array ) );
}

template <typename... T>
JSON Array( T... args ) {
    JSON arr = JSON::Make( JSON::Class::Array );
    arr.append( args... );
    return std::move( arr );
}

JSON Object() {
    return std::move( JSON::Make( JSON::Class::Object ) );
}

std::ostream& operator<<( std::ostream &os, const JSON &json ) {
    os << json.dump();
    return os;
}

namespace {
    JSON parse_next( const string &, size_t & );

    void consume_ws( const string &str, size_t &offset ) {
        while( isspace( str[offset] ) ) ++offset;
    }

    JSON parse_object( const string &str, size_t &offset ) {
        JSON Object = JSON::Make( JSON::Class::Object );

        ++offset;
        consume_ws( str, offset );
        if( str[offset] == '}' ) {
            ++offset; return std::move( Object );
        }

        while( true ) {
            JSON Key = parse_next( str, offset );
            consume_ws( str, offset );
            if( str[offset] != ':' ) {
                std::cerr << "Error: Object: Expected colon, found '" << str[offset] << "'\n";
                break;
            }
            consume_ws( str, ++offset );
            JSON Value = parse_next( str, offset );
            Object[Key.ToString()] = Value;
            
            consume_ws( str, offset );
            if( str[offset] == ',' ) {
                ++offset; continue;
            }
            else if( str[offset] == '}' ) {
                ++offset; break;
            }
            else {
                std::cerr << "ERROR: Object: Expected comma, found '" << str[offset] << "'\n";
                break;
            }
        }

        return std::move( Object );
    }

    JSON parse_array( const string &str, size_t &offset ) {
        JSON Array = JSON::Make( JSON::Class::Array );
        unsigned index = 0;
        
        ++offset;
        consume_ws( str, offset );
        if( str[offset] == ']' ) {
            ++offset; return std::move( Array );
        }

        while( true ) {
            Array[index++] = parse_next( str, offset );
            consume_ws( str, offset );

            if( str[offset] == ',' ) {
                ++offset; continue;
            }
            if( str[offset] == ']' ) {
                ++offset; break;
            }
            else {
                std::cerr << "ERROR: Array: Expected comma, found '" << str[offset] << "'\n";
                break;
            }
        }

        return std::move( Array );
    }

    JSON parse_string( const string &str, size_t &offset ) {
        JSON String;
        string val;
        for( char c = str[++offset]; c != '\"' ; c = str[++offset] ) {
            if( c == '\\' )
                c = str[++offset];
            val += c;
        }
        ++offset;
        String = val;
        return std::move( String );
    }

    JSON parse_number( const string &str, size_t &offset ) {
        JSON Number;
        string val;
        char c;
        bool isDouble = false;
        while( true ) {
            c = str[offset++];
            if( (c == '-') || (c >= '0' && c <= '9') )
                val += c;
            else if( c == '.' ) {
                val += c; 
                isDouble = true;
            }
            else {
                --offset;
                break;
            }
        }
        if( isDouble )
            Number = std::stod( val );
        else
            Number = std::stol( val );
        return std::move( Number );
    }

    JSON parse_bool( const string &str, size_t &offset ) {
        JSON Bool;
        Bool = (str[offset] == 't');
        offset += (Bool.ToBool() ? 4 : 5);
        return std::move( Bool );
    }

    JSON parse_null( const string &str, size_t &offset ) {
        JSON Null;
        offset += 4;
        return std::move( Null );
    }

    JSON parse_next( const string &str, size_t &offset ) {
        char value;
        consume_ws( str, offset );
        value = str[offset];
        switch( value ) {
            case '[' : return std::move( parse_array( str, offset ) );
            case '{' : return std::move( parse_object( str, offset ) );
            case '\"': return std::move( parse_string( str, offset ) );
            case 't' :
            case 'f' : return std::move( parse_bool( str, offset ) );
            case 'n' : return std::move( parse_null( str, offset ) );
            default  : if( ( value <= '9' && value >= '0' ) || value == '-' )
                           return std::move( parse_number( str, offset ) );
        }
        std::cerr << "Error parsing JSON starting with char '" << value << "'\n";
        return JSON();
    }
}

JSON JSON::Load( const string &str ) {
    size_t offset = 0;
    return std::move( parse_next( str, offset ) );
}

} // End Namespace json
