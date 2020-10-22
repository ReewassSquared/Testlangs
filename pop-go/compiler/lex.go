package compiler

import (
	"fmt"
	"io"
	"strconv"
	"unicode"
	"unicode/utf8"
)

type token uint32
type lexmode uint8

const __TRACE__ = true

const LexerBufferMin = 12
const LexerBufferMax = 20
const readCountMax = 10

const (
	LEXMODE_POPCODE = iota + 1
	LEXMODE_POPQUOT
	LEXMODE_POPQAQT
	LEXMODE_HASKELL
)

const TOK_UNK = 0

const (
	TOKPOP_UNDEFN = iota + 1
	TOKPOP_LPAREN
	TOKPOP_RPAREN
	TOKPOP_NUMLIT
	TOKPOP_STRLIT
	TOKPOP_IDENTF
	TOKPOP_ADDOPR
	TOKPOP_SUBOPR
	TOKPOP_MULOPR
	TOKPOP_DEFINE
	TOKPOP_DIVOPR
	TOKPOP_CONDIT
	TOKPOP_ZEROIF
	TOKPOP_IFWORD
	TOKPOP_ELSEWD
	TOKPOP_ABSOPR
	TOKPOP_GTCOMP
	TOKPOP_GECOMP
	TOKPOP_EQCOMP
	TOKPOP_LECOMP
	TOKPOP_LTCOMP
	TOKPOP_INCOPR
	TOKPOP_DECOPR
	TOKPOP_NECOMP
	TOKPOP_PRTOPR
	TOKPOP_BVTRUE
	TOKPOP_BVFALS
	TOKPOP_LETLET
	TOKPOP_BOXBOX
	TOKPOP_UNBOXE
	TOKPOP_CONSLT
	TOKPOP_CARCAR
	TOKPOP_CDRCDR
	TOKPOP_EMPTYL
	TOKPOP_FNPROC
	TOKPOP_FNCALL
	TOKPOP_LAMBDA
	TOKPOP_LQUOTE
	TOKPOP_QQUOTE
	TOKPOP_PERIOD
	TOKPOP_UNQUOT
	TOKPOP_UNQTLS
	TOKPOP_SYMBOL
	TOKPOP_STREQL
	TOKPOP_CHKSTR
	TOKPOP_CHKLST
	TOKPOP_CHKELS
	TOKPOP_CHKINT
	TOKPOP_CHKFLG
	TOKPOP_CHKBOO
	TOKPOP_CHKSYM
	TOKPOP_SYMEQL
	TOKPOP_CHKPRC
	TOKPOP_CHKBOX
	TOKPOP_CHKBYT
	TOKPOP_ANDAND
	TOKPOP_OROROR
	TOKPOP_LENGTH
	TOKPOP_LISTBD
	TOKPOP_MODMOD
	TOKPOP_NOTNOT
	TOKPOP_BITAND
	TOKPOP_BITLOR
	TOKPOP_BITSHL
	TOKPOP_BITSHR
	TOKPOP_BITXOR
	TOKPOP_MATCHE
	TOKPOP_QUMARK
	TOKPOP_CHKCNS
	TOKPOP_MTCHDF
	TOKPOP_SUPRSS
	TOKPOP_MUTSET
	TOKPOP_FLOPEN
	TOKPOP_FLWRIT
	TOKPOP_FLCLOS
	TOKPOP_SYMAPP
	TOKPOP_ENDOFL
)

const sentinel = utf8.RuneSelf

type Token struct {
	l, c int
	tok  token
	val  string
}

type Lexer struct {
	b, r, e    int
	chw        int
	ch         rune
	buf        []byte
	tok        token
	val        string
	l, c       int
	scan       io.Reader
	mode       lexmode
	bsize      int
	ioerr      error
	toks       []Token
	be, bq, bl int
	eof        bool
}

func (s *Lexer) init(r io.Reader, mode lexmode) {
	s.b, s.r, s.e = -1, 0, 0
	s.l, s.c = 1, 0
	s.buf = make([]byte, 4096)
	s.chw = -1
	s.ch = ' '
	s.buf[0] = sentinel
	s.val, s.mode = "", mode
	s.tok = TOK_UNK
	s.ioerr = nil
	s.bsize = 12
	s.be, s.bq, s.bl = -1, -1, -1
	s.eof = false
}

func (s *Lexer) lex() {
	for !s.eof {
		s.nextch()
	}

	if __TRACE__ {
		for _, v := range s.toks {
			fmt.Printf("[%04d:%04d] %16s %32s\n", v.l, v.c, token_name(v.tok), v.val)
		}
	}
}

func (s *Lexer) start() { s.b = s.r - s.chw }
func (s *Lexer) stop()  { s.b = -1 }
func (s *Lexer) segment() []byte {
	return s.buf[s.b : s.r-s.chw]
}

func (s *Lexer) errorf(msg string) {
	fmt.Printf("[%d:%d] %s\n", s.l, s.c, msg)
	//panic("")
}

func (s *Lexer) nextch() {
redo:
	s.c += int(s.chw)
	if s.ch == '\n' {
		s.l++
		s.c = 0
	}

	//first test for ASCII
	if s.ch = rune(s.buf[s.r]); s.ch < sentinel {
		s.r++
		s.chw = 1
		if s.ch == 0 {
			s.errorf("NUL")
			goto redo
		}
		return
	}

	for s.e-s.r < utf8.UTFMax && !utf8.FullRune(s.buf[s.r:s.e]) && s.ioerr == nil {
		s.fill()
	}

	//EOF
	if s.r == s.e || s.ioerr == io.EOF {
		if s.ioerr != io.EOF {
			s.errorf("IO Error: " + s.ioerr.Error())
			s.ioerr = nil
		}
		s.ch = -1
		s.chw = 0
		s.eof = true
		return
	}

	s.ch, s.chw = utf8.DecodeRune(s.buf[s.r:s.e])
	s.r += s.chw
	if s.ch == utf8.RuneError && s.chw == 1 {
		s.errorf("invalid UTF-8 encoding!")
		goto redo
	}

	//WATCH OUT FOR BOM
	if s.ch == 0xfeff {
		if s.l > 0 || s.c > 0 {
			s.errorf("invalid UFT-8 byte-order mark in middle of file")
		}
		goto redo
	}
}

func (s *Lexer) fill() {
	b := s.r
	if s.b >= 0 {
		b = s.b
		s.b = 0
	}
	content := s.buf[b:s.e]
	if len(content)*2 > len(s.buf) {
		s.bsize++
		if s.bsize > LexerBufferMax {
			s.bsize = LexerBufferMax
		}
		s.buf = make([]byte, 1<<s.bsize)
		copy(s.buf, content)
	} else if b > 0 {
		copy(s.buf, content)
	}
	s.r -= b
	s.e -= b

	for i := 0; i < readCountMax; i++ {
		var n int
		n, s.ioerr = s.scan.Read(s.buf[s.e : len(s.buf)-1])
		if n < 0 {
			panic("negative read!") //invalid io.Reader
		}
		if n > 0 || s.ioerr != nil {
			s.e += n
			s.buf[s.e] = sentinel
			return
		}
	}

	s.buf[s.e] = sentinel
	s.ioerr = io.ErrNoProgress
}

func (s *Lexer) nextc() {
	if s.eof {
		return
	}
	for s.be >= 0 {
		mode := s.mode
		ctrl := s.next()
		if s.mode != mode {
			switch s.mode {
			case LEXMODE_POPQUOT:
				s.nextq()
				break
			case LEXMODE_POPQAQT:
				s.nextqq()
				break
			default:
				s.errorf("unknown lexer mode!")
			}
		}
		if ctrl {
			return
		}
	}
}

func (s *Lexer) nextqq() {
	if s.eof {
		return
	}
	bl := s.bl
	s.bl = 0
	ignorenext := false
	for s.bl >= 0 {
		ignorenext = false
		s.stop()
		for !s.eof && (s.ch == ' ' || s.ch == '\t' || s.ch == '\r' || s.ch == '\n') {
			s.nextch()
		}
		s.start()
		tok := Token{}
		tok.l, tok.c = s.l, s.c
		for !s.eof && s.ch != ' ' && s.ch != '\t' && s.ch != '\r' && s.ch != '\n' && s.ch != '(' && s.ch != '[' && s.ch != ']' && s.ch != ')' && s.ch != ',' && !(s.bl > 0 && s.ch == '.') {
			s.nextch()
		}

		switch s.ch {
		case -1, 0:
			s.eof = true
			s.tok = TOKPOP_ENDOFL
			s.bl = -1
			break
		case '(', '[':
			s.nextch()
			s.bl++
			tok.tok = TOKPOP_LPAREN
			break
		case ')', ']':
			s.bl--
			val := s.segment()
			if len(val) != 0 {
				/* assume symbol lies underneath */
				tok.val = string(val)
				tok.tok = TOKPOP_SYMBOL
				s.toks = append(s.toks, tok)
				tok.l, tok.c = s.l, s.c
			}
			s.nextch()
			tok.tok = TOKPOP_RPAREN
			if s.bl <= 0 {
				s.bl = -1
			}
			break
		case '.':
			if s.bl > 0 {
				s.nextch()
				tok.tok = TOKPOP_PERIOD
				break
			} else {
				s.nextch()
				ignorenext = true
				tok.tok = TOKPOP_SYMBOL
				tok.val = string(s.segment())
				if s.bl <= 0 {
					s.bl = -1
				}
				break
			}
		case ',':
			s.nextch()
			tok.tok = TOKPOP_UNQUOT
			s.toks = append(s.toks, tok)
			s.mode = LEXMODE_POPCODE
			s.nextc()
			s.mode = LEXMODE_POPQAQT
			if s.bl <= 0 {
				s.bl = -1
				return
			} else {
				ignorenext = true
			}
		default:
			s.nextch()
			tok.tok = TOKPOP_SYMBOL
			tok.val = string(s.segment())
			if s.bl <= 0 {
				s.bl = -1
			}
			break
		}

		if !ignorenext {
			s.toks = append(s.toks, tok)
		}
	}
	s.bl = bl
}

func (s *Lexer) nextq() {
	if s.eof {
		return
	}
	bq := s.bq
	s.bq = 0
	ignorenext := false
	for s.bq >= 0 {
		ignorenext = false
		s.stop()
		for !s.eof && (s.ch == ' ' || s.ch == '\t' || s.ch == '\r' || s.ch == '\n') {
			s.nextch()
		}
		s.start()
		tok := Token{}
		tok.l, tok.c = s.l, s.c
		for !s.eof && s.ch != ' ' && s.ch != '\t' && s.ch != '\r' && s.ch != '\n' && s.ch != '(' && s.ch != '[' && s.ch != ']' && s.ch != ')' && !(s.bl > 0 && s.ch == '.') {
			s.nextch()
		}

		switch s.ch {
		case -1, 0:
			s.eof = true
			s.tok = TOKPOP_ENDOFL
			s.bq = -1
			break
		case '(', '[':
			s.nextch()
			s.bq++
			tok.tok = TOKPOP_LPAREN
			break
		case ')', ']':
			s.bq--
			val := s.segment()
			if len(val) != 0 {
				/* assume symbol lies underneath */
				tok.val = string(val)
				tok.tok = TOKPOP_SYMBOL
				s.toks = append(s.toks, tok)
				tok.l, tok.c = s.l, s.c
			}
			s.nextch()
			tok.tok = TOKPOP_RPAREN
			if s.bl <= 0 {
				s.bl = -1
			}
			break
		case '.':
			if s.bq > 0 {
				s.nextch()
				tok.tok = TOKPOP_PERIOD
				break
			} else {
				s.nextch()
				ignorenext = true
				tok.tok = TOKPOP_SYMBOL
				tok.val = string(s.segment())
				if s.bq <= 0 {
					s.bq = -1
				}
				break
			}
		default:
			s.nextch()
			tok.tok = TOKPOP_SYMBOL
			tok.val = string(s.segment())
			if s.bq <= 0 {
				s.bq = -1
			}
			break
		}

		if !ignorenext {
			s.toks = append(s.toks, tok)
		}
	}
	s.bq = bq
}

/* fills the tokstring with at least one or more tokens */
/* returns true only if s-expression is complete */
func (s *Lexer) next() bool {
	rv := false
	if s.eof {
		return true
	}

redonext:
	s.stop()
	for s.ch == ' ' || s.ch == '\t' || s.ch == '\n' || s.ch == '\r' {
		s.nextch()
	}

	s.start()

	tok := Token{}
	tok.l, tok.c = s.l, s.c

	if isLetter(s.ch) || s.ch >= utf8.RuneSelf && s.atIdentChar(true) {
		s.nextch()
		tok.tok = TOKPOP_IDENTF
		tok.val = s.ident()
		return rv
	}

	switch s.ch {
	case -1, 0:
		s.eof = true
		s.tok = TOKPOP_ENDOFL
		s.be = -1
		rv = true
		break
	case '0', '1', '2', '3', '4', '5', '6', '7', '8', '9':
		tok.tok = TOKPOP_NUMLIT
		tok.val = s.number(false)
		if s.be <= 0 {
			rv = true
		}
		break
	case '"':
		tok.tok = TOKPOP_STRLIT
		tok.val = s.stdString()
		if s.be <= 0 {
			rv = true
		}
		break
	case '`':
		tok.tok = TOKPOP_QQUOTE
		s.mode = LEXMODE_POPQAQT
		if s.be <= 0 {
			rv = true
		}
		break
	case '\'':
		tok.tok = TOKPOP_LQUOTE
		s.mode = LEXMODE_POPQUOT
		if s.be <= 0 {
			rv = true
		}
		break
	case '(', '[':
		pch := s.ch
		s.nextch()
		if pch == '(' && s.ch == ')' {
			s.nextch()
			tok.tok = TOKPOP_EMPTYL
			break
		}
		tok.tok = TOKPOP_LPAREN
		s.be++
		break
	case ')', ']':
		s.nextch()
		tok.tok = TOKPOP_RPAREN
		s.be--
		if s.be <= 0 {
			rv = true
		}
		break
	case '+':
		s.nextch()
		tok.tok = TOKPOP_ADDOPR
		break
	case '-':
		s.nextch()
		tok.tok = TOKPOP_SUBOPR
		break
	case '*':
		s.nextch()
		tok.tok = TOKPOP_MULOPR
		break
	case '/':
		s.nextch()
		tok.tok = TOKPOP_DIVOPR
		break
	case ',':
		s.nextch()
		tok.tok = TOKPOP_UNQUOT
		break
	case ';':
		for s.ch != '\n' {
			s.nextch()
		}
		goto redonext
	case '>':
		s.nextch()
		if s.ch == '=' {
			s.nextch()
			tok.tok = TOKPOP_GECOMP
			break
		}
		tok.tok = TOKPOP_GTCOMP
		break
	case '<':
		s.nextch()
		if s.ch == '=' {
			s.nextch()
			tok.tok = TOKPOP_LECOMP
			break
		}
		tok.tok = TOKPOP_LTCOMP
		break
	case '=':
		s.nextch()
		tok.tok = TOKPOP_EQCOMP
		break
	case '~':
		s.nextch()
		tok.tok = TOKPOP_NECOMP
		break
	case '?':
		s.nextch()
		if s.ch == ' ' || s.ch == '\t' || s.ch == '\n' || s.ch == '\r' {
			tok.tok = TOKPOP_QUMARK
			break
		}
		tok.tok = TOKPOP_IDENTF
		tok.val = s.ident()
		break
	case '#':
		s.nextch()
		if s.ch == 't' {
			s.nextch()
			s.tok = TOKPOP_BVTRUE
			break
		} else if s.ch == 'f' {
			s.nextch()
			s.tok = TOKPOP_BVFALS
			break
		} else if s.ch == ';' {
			s.nextch()
			s.tok = TOKPOP_SUPRSS
			break
		}
		for !s.eof && s.ch != ' ' && s.ch != '\n' && s.ch != '\t' && s.ch != '\r' && s.ch != '(' && s.ch != ')' && s.ch != '[' && s.ch != ']' && s.ch != ';' && s.ch != '`' && s.ch != '\'' && s.ch != '"' {
			s.nextch()
		}
		s.errorf(fmt.Sprintf("unknown octothorpe value: #%s", string(s.segment())))
		goto redonext
	default:
		if isLetter(s.ch) {
			tok.tok = TOKPOP_IDENTF
			tok.val = s.ident()
			break
		} else {
			for !s.eof && s.ch != ' ' && s.ch != '\n' && s.ch != '\t' && s.ch != '\r' && s.ch != '(' && s.ch != ')' && s.ch != '[' && s.ch != ']' && s.ch != ';' && s.ch != '`' && s.ch != '\'' && s.ch != '"' {
				s.nextch()
			}
			s.errorf(fmt.Sprintf("unknown token: %s", string(s.segment())))
			goto redonext
		}
	}

	if s.eof {
		tok.tok = TOKPOP_ENDOFL
	}
	s.toks = append(s.toks, tok)
	s.tok = tok.tok
	s.val = tok.val
	return rv
}

func lower(ch rune) rune     { return ('a' - 'A') | ch } // returns lower-case ch iff ch is ASCII letter
func isLetter(ch rune) bool  { return 'a' <= lower(ch) && lower(ch) <= 'z' || ch == '_' }
func isDecimal(ch rune) bool { return '0' <= ch && ch <= '9' }
func isHex(ch rune) bool     { return '0' <= ch && ch <= '9' || 'a' <= lower(ch) && lower(ch) <= 'f' }

func (s *Lexer) atIdentChar(first bool) bool {
	switch {
	case s.ch == -1 || s.ch == 0:
		return false
	case unicode.IsLetter(s.ch) || s.ch == '_':
	case unicode.IsDigit(s.ch) && !first:
		s.errorf(fmt.Sprintf("identifier cannot begin with digit %#U", s.ch))
		return false
	case s.ch >= utf8.RuneSelf:
		s.errorf(fmt.Sprintf("invalid character %#U in identifier", s.ch))
		return false
	default:
		return false
	}
	return true
}

func (s *Lexer) number(seenPoint bool) string {
	for isDecimal(s.ch) {
		s.nextch()
	}
	return string(s.segment())

}

func (s *Lexer) ident() string {
	for !s.eof && s.ch != ' ' && s.ch != '\n' && s.ch != '\t' && s.ch != '\r' && s.ch != '(' && s.ch != ')' && s.ch != '[' && s.ch != ']' && s.ch != ';' && s.ch != '`' && s.ch != '\'' && s.ch != '"' {
		s.nextch()
	}
	return string(s.segment())
}

func baseName(base int) string {
	switch base {
	case 2:
		return "binary"
	case 8:
		return "octal"
	case 10:
		return "decimal"
	case 16:
		return "hexadecimal"
	}
	panic("invalid base")
}

func (s *Lexer) stdString() string {
	s.nextch()

	for {
		if s.ch == '"' {
			s.nextch()
			break
		}
		if s.ch == '\\' {
			s.nextch()
			if !s.escape('"') {

			}
			continue
		}
		if s.ch <= 0 || s.eof {
			s.errorf("string not terminated")
			break
		}
		s.nextch()
	}

	rv, err := strconv.Unquote(string(s.segment()))
	if err != nil {
		s.errorf("invalid string literal: " + err.Error())
	}
	return rv
}

func (s *Lexer) escape(quote rune) bool {
	var n int
	var base, max uint32

	switch s.ch {
	case quote, 'a', 'b', 'f', 'n', 'r', 't', 'v', '\\':
		s.nextch()
		return true
	case '0', '1', '2', '3', '4', '5', '6', '7':
		n, base, max = 3, 8, 255
	case 'x':
		s.nextch()
		n, base, max = 2, 16, 255
	case 'u':
		s.nextch()
		n, base, max = 4, 16, unicode.MaxRune
	case 'U':
		s.nextch()
		n, base, max = 8, 16, unicode.MaxRune
	default:
		if s.ch < 0 {
			return true // complain in caller about EOF
		}
		s.errorf("unknown escape")
		return false
	}

	var x uint32
	for i := n; i > 0; i-- {
		if s.ch < 0 {
			return true // complain in caller about EOF
		}
		d := base
		if isDecimal(s.ch) {
			d = uint32(s.ch) - '0'
		} else if 'a' <= lower(s.ch) && lower(s.ch) <= 'f' {
			d = uint32(lower(s.ch)) - 'a' + 10
		}
		if d >= base {
			s.errorf(fmt.Sprintf("invalid character %q in %s escape", s.ch, baseName(int(base))))
			return false
		}
		// d < base
		x = x*base + d
		s.nextch()
	}

	if x > max && base == 8 {
		s.errorf(fmt.Sprintf("octal escape value %d > 255", x))
		return false
	}

	if x > max || 0xD800 <= x && x < 0xE000 /* surrogate range */ {
		s.errorf(fmt.Sprintf("escape is invalid Unicode code point %#U", x))
		return false
	}

	return true
}

func token_name(tok token) string {
	switch tok {
	case TOKPOP_UNDEFN:
		return "undefined"
	case TOKPOP_LPAREN:
		return "("
	case TOKPOP_RPAREN:
		return ")"
	case TOKPOP_NUMLIT:
		return "number"
	case TOKPOP_STRLIT:
		return "string"
	case TOKPOP_IDENTF:
		return "identifier"
	case TOKPOP_ADDOPR:
		return "+"
	case TOKPOP_SUBOPR:
		return "-"
	case TOKPOP_MULOPR:
		return "*"
	case TOKPOP_DEFINE:
		return "define"
	case TOKPOP_DIVOPR:
		return "/"
	case TOKPOP_CONDIT:
		return "cond"
	case TOKPOP_ZEROIF:
		return "zero?"
	case TOKPOP_IFWORD:
		return "if"
	case TOKPOP_ELSEWD:
		return "else"
	case TOKPOP_ABSOPR:
		return "abs"
	case TOKPOP_GTCOMP:
		return ">"
	case TOKPOP_GECOMP:
		return ">="
	case TOKPOP_EQCOMP:
		return "="
	case TOKPOP_LECOMP:
		return "<="
	case TOKPOP_LTCOMP:
		return "<"
	case TOKPOP_INCOPR:
		return "add1"
	case TOKPOP_DECOPR:
		return "sub1"
	case TOKPOP_NECOMP:
		return "~"
	case TOKPOP_PRTOPR:
		return "print"
	case TOKPOP_BVTRUE:
		return "#t"
	case TOKPOP_BVFALS:
		return "#f"
	case TOKPOP_LETLET:
		return "let"
	case TOKPOP_BOXBOX:
		return "box"
	case TOKPOP_UNBOXE:
		return "unbox"
	case TOKPOP_CONSLT:
		return "cons"
	case TOKPOP_CARCAR:
		return "car"
	case TOKPOP_CDRCDR:
		return "cdr"
	case TOKPOP_EMPTYL:
		return "()"
	case TOKPOP_FNPROC:
		return "fun"
	case TOKPOP_FNCALL:
		return "call"
	case TOKPOP_LAMBDA:
		return "lambda"
	case TOKPOP_LQUOTE:
		return "'"
	case TOKPOP_QQUOTE:
		return "`"
	case TOKPOP_PERIOD:
		return "."
	case TOKPOP_UNQUOT:
		return ","
	case TOKPOP_UNQTLS:
		return ",@"
	case TOKPOP_SYMBOL:
		return "symbol"
	case TOKPOP_STREQL:
		return "string=?"
	case TOKPOP_CHKSTR:
		return "string?"
	case TOKPOP_CHKLST:
		return "list?"
	case TOKPOP_CHKELS:
		return "emptylist?"
	case TOKPOP_CHKINT:
		return "integer?"
	case TOKPOP_CHKFLG:
		return "flag?"
	case TOKPOP_CHKBOO:
		return "boolean?"
	case TOKPOP_CHKSYM:
		return "symbol?"
	case TOKPOP_SYMEQL:
		return "symbol=?"
	case TOKPOP_CHKPRC:
		return "proc?"
	case TOKPOP_CHKBOX:
		return "box?"
	case TOKPOP_CHKBYT:
		return "bytes?"
	case TOKPOP_ANDAND:
		return "and"
	case TOKPOP_OROROR:
		return "or"
	case TOKPOP_LENGTH:
		return "length"
	case TOKPOP_LISTBD:
		return "list"
	case TOKPOP_MODMOD:
		return "mod"
	case TOKPOP_NOTNOT:
		return "not"
	case TOKPOP_BITAND:
		return "bitand"
	case TOKPOP_BITLOR:
		return "bitor"
	case TOKPOP_BITSHL:
		return "bitshl"
	case TOKPOP_BITSHR:
		return "bitshr"
	case TOKPOP_BITXOR:
		return "bitxor"
	case TOKPOP_MATCHE:
		return "match"
	case TOKPOP_QUMARK:
		return "?"
	case TOKPOP_CHKCNS:
		return "cons?"
	case TOKPOP_MTCHDF:
		return "_"
	case TOKPOP_SUPRSS:
		return "#;"
	case TOKPOP_MUTSET:
		return "set!"
	case TOKPOP_FLOPEN:
		return "file-open"
	case TOKPOP_FLWRIT:
		return "file-write"
	case TOKPOP_FLCLOS:
		return "file-close"
	case TOKPOP_SYMAPP:
		return "symbol-append"
	case TOKPOP_ENDOFL:
		return "eof"
	default:
		return "<unk>"
	}
}
