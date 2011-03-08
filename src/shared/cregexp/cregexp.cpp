#include<cregexp/cregexp.h>
#include<unicode/UnicodeTools.h>

/////////////////////////////////////////////////////////////////////////////
//
SRegInfo::SRegInfo()
{
  next = prev = parent = null;
  un.param = null;
  op = ReEmpty;
  param0 = param1 = 0;
};
SRegInfo::~SRegInfo()
{
  if (next) delete next;
  if (un.param) switch(op){
    case ReEnum:
    case ReNEnum:
      delete un.charclass;
      break;
    case ReWord:
      delete un.word;
      break;
#ifdef NAMED_MATCHES_IN_HASH
    case ReNamedBrackets:
    case ReBkBrackName:
      if(namedata) delete namedata;
#endif
    default:
      if (op > ReBlockOps && (op < ReSymbolOps
          || op == ReBrackets || op == ReNamedBrackets))
        delete un.param;
      break;
  };
};

////////////////////////////////////////////////////////////////////////////
// CRegExp class
void CRegExp::setWow64(bool wow64)
{
#ifdef _WIN32
  if (wow64)  adr_so=0x00094444;
  else adr_so=0x00034444;
#else
  adr_so=0;
#endif
}
void CRegExp::init()
{
  tree_root = 0;
  positionMoves = false;
  error = EERROR;
  firstChar = 0;
  cMatch = 0;
  global_pattern = 0;
#ifdef COLORERMODE
  backRE = 0;
  backStr = null;
  backTrace = null;
#endif
#ifndef NAMED_MATCHES_IN_HASH
  cnMatch = 0;
#else
  namedMatches = 0;
#endif
};
CRegExp::CRegExp()
{
  init();
};
CRegExp::CRegExp(const String *text)
{
  init();
  if (text) setRE(text);
};
CRegExp::~CRegExp()
{
  if (tree_root) delete tree_root;
#ifndef NAMED_MATCHES_IN_HASH
  for(int bp = 0; bp < cnMatch; bp++)
    if(brnames[bp]) delete brnames[bp];
#endif
};

EError CRegExp::setRELow(const String &expr)
{
  int len = expr.length();
  if (!len) return EERROR;

  if (tree_root) delete tree_root;
  tree_root = null;
#ifndef NAMED_MATCHES_IN_HASH
  for(int bp = 0; bp < cnMatch; bp++)
    if(brnames[bp]) delete brnames[bp];
#endif

  cMatch = 0;
#ifndef NAMED_MATCHES_IN_HASH
  cnMatch = 0;
#endif
  endChange = startChange = false;
  int start = 0;
  while (Character::isWhitespace(expr[start])) start++;
  if (expr[start] == '/') start++;
  else return ESYNTAX;

  bool ok = false;
  ignoreCase = extend = singleLine = multiLine = false;
  for (int i = len-1; i >= start && !ok; i--)
  if (expr[i] == '/'){
    for (int j = i+1; j < len; j++){
      if (expr[j] == 'i') ignoreCase = true;
      if (expr[j] == 'x') extend = true;
      if (expr[j] == 's') singleLine = true;
      if (expr[j] == 'm') multiLine = true;
    };
    len = i-start;
    ok = true;
  };
  if (!ok) return ESYNTAX;

  // making tree structure
  tree_root = new SRegInfo;
  tree_root->op = ReBrackets;
  tree_root->un.param = new SRegInfo;
  tree_root->un.param->parent = tree_root;
  tree_root->param0 = cMatch++;

  int endPos;
  EError err = setStructs(tree_root->un.param, DString(&expr, start, len), endPos);
  if (endPos != len) err = EBRACKETS;

  if (err) return err;
  optimize();
  return EOK;
};


void CRegExp::optimize()
{
SRegInfo *next = tree_root;
  firstChar = BAD_WCHAR;
  firstMetaChar = ReBadMeta;
  while(next){
    if (next->op == ReBrackets){
      next = next->un.param;
      continue;
    };
/*    if (next->op == ReMetaSymb &&
        next->un.metaSymbol >= ReWBound && next->un.metaSymbol < ReChrLast){
      next = next->next;
      continue;
    };*/
    if (next->op == ReMetaSymb){
      if (next->un.metaSymbol != ReSoL && next->un.metaSymbol != ReWBound) break;
      firstMetaChar = next->un.metaSymbol;
      break;
    };
    if (next->op == ReSymb){
      firstChar = next->un.symbol;
      break;
    };
    if (next->op == ReWord){
      firstChar = (*next->un.word)[0];
    }
    break;
  };
};

EError CRegExp::setStructs(SRegInfo *&re, const String &expr, int &retPos)
{
SRegInfo *next, *temp;

  retPos = 0;
  if (!expr.length()) return EOK;
  retPos = -1;

  next = re;
  for(int i = 0; i < expr.length(); i++){
    // simple character
    if (extend && Character::isWhitespace(expr[i])) continue;
    // context return
    if (expr[i] == ')'){
      retPos = i;
      break;
    };
    // next element
    if (i != 0){
      next->next = new SRegInfo;
      next->next->parent = next->parent;
      next->next->prev = next;
      next = next->next;
    };
    // Escape symbol
    if (expr[i] == '\\'){
      String *br_name;
      int blen;
      switch (expr[i+1]){
        case 'd':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReDigit;
          break;
        case 'D':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReNDigit;
          break;
        case 'w':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReWordSymb;
          break;
        case 'W':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReNWordSymb;
          break;
        case 's':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReWSpace;
          break;
        case 'S':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReNWSpace;
          break;
        case 'u':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReUCase;
          break;
        case 'l':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReNUCase;
          break;
        case 't':
          next->op = ReSymb;
          next->un.symbol = '\t';
          break;
        case 'n':
          next->op = ReSymb;
          next->un.symbol = '\n';
          break;
        case 'r':
          next->op = ReSymb;
          next->un.symbol = '\r';
          break;
        case 'b':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReWBound;
          break;
        case 'B':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReNWBound;
          break;
        case 'c':
          next->op = ReMetaSymb;
          next->un.metaSymbol = RePreNW;
          break;
#ifdef COLORERMODE
        case 'm':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReStart;
          break;
        case 'M':
          next->op = ReMetaSymb;
          next->un.metaSymbol = ReEnd;
          break;
#ifndef NAMED_MATCHES_IN_HASH
        case 'y':
        case 'Y':
          next->op = (expr[i+1] =='y'?ReBkTrace:ReBkTraceN);
          next->param0 = UnicodeTools::getHex(expr[i+2]);
          if (next->param0 != -1){
            i++;
          }else{
            next->op = (expr[i+1] =='y'?ReBkTraceName:ReBkTraceNName);
            br_name = UnicodeTools::getCurlyContent(expr, i+2);
            if (br_name == null) return ESYNTAX;
            if (!backRE){
              delete br_name;
              return EERROR;
            }
            next->param0 = backRE->getBracketNo(br_name);
            blen = br_name->length();
            delete br_name;
            if (next->param0 == -1) return ESYNTAX;
            i += blen+2;
          };
          break;
#endif // COLORERMODE
#endif // NAMED_MATCHES_IN_HASH

        case 'p':  // \p{name}
          next->op = ReBkBrackName;
          br_name = UnicodeTools::getCurlyContent(expr, i+2);
          if (br_name == null) return ESYNTAX;
          blen = br_name->length();
#ifndef NAMED_MATCHES_IN_HASH
          next->param0 = getBracketNo(br_name);
          delete br_name;
          if (next->param0 == -1) return ESYNTAX;
#else
          if(br_name->length() && namedMatches && !namedMatches->getItem(br_name)){
            delete br_name;
            return EBRACKETS;
          };
          next->param0 = 0;
          next->namedata = new SString(br_name);
          delete br_name;
#endif
          i += blen+2;
          break;
        default:
          next->op = ReBkBrack;
          next->param0 = UnicodeTools::getHex(expr[i+1]);
          if (next->param0 < 0 || next->param0 > 9){
            int retEnd;
            next->op = ReSymb;
            next->un.symbol = UnicodeTools::getEscapedChar(expr, i, retEnd);
            if (next->un.symbol == BAD_WCHAR) return ESYNTAX;
            i = retEnd-1;
          };
          break;
      };
      i++;
      continue;
    };

    if (expr[i] == '.'){
      next->op = ReMetaSymb;
      next->un.metaSymbol = ReAnyChr;
      continue;
    };
    if (expr[i] == '^'){
      next->op = ReMetaSymb;
      next->un.metaSymbol = ReSoL;
      continue;
    };
    if (expr[i] == '$'){
      next->op = ReMetaSymb;
      next->un.metaSymbol = ReEoL;
      continue;
    };
#ifdef COLORERMODE
    if (expr[i] == '~'){
      next->op = ReMetaSymb;
      next->un.metaSymbol = ReSoScheme;
      continue;
    };
#endif

    next->un.param = 0;
    next->param0 = 0;

    if (expr.length() > i+2){
      if (expr[i] == '?' && expr[i+1] == '#' &&
          expr[i+2] >= '0' && expr[i+2] <= '9'){
        next->op = ReBehind;
        next->param0 = UnicodeTools::getHex(expr[i+2]);
        i += 2;
        continue;
      };
      if (expr[i] == '?' && expr[i+1] == '~' &&
          expr[i+2]>='0' && expr[i+2]<='9'){
        next->op = ReNBehind;
        next->param0 = UnicodeTools::getHex(expr[i+2]);
        i += 2;
        continue;
      };
    };
    if (expr.length() > i+1){
      if (expr[i] == '*' && expr[i+1] == '?'){
        next->op = ReNGRangeN;
        next->s = 0;
        i++;
        continue;
      };
      if (expr[i] == '+' && expr[i+1] == '?'){
        next->op = ReNGRangeN;
        next->s = 1;
        i++;
        continue;
      };
      if (expr[i] == '?' && expr[i+1] == '='){
        next->op = ReAhead;
        i++;
        continue;
      };
      if (expr[i] == '?' && expr[i+1] == '!'){
        next->op = ReNAhead;
        i++;
        continue;
      };
      if (expr[i] == '?' && expr[i+1] == '?'){
        next->op = ReNGRangeNM;
        next->s = 0;
        next->e = 1;
        i++;
        continue;
      };
    };

    if (expr[i] == '*'){
      next->op = ReRangeN;
      next->s = 0;
      continue;
    };
    if (expr[i] == '+'){
      next->op = ReRangeN;
      next->s = 1;
      continue;
    };
    if (expr[i] == '?'){
      next->op = ReRangeNM;
      next->s = 0;
      next->e = 1;
      continue;
    };
    if (expr[i] == '|'){
      next->op = ReOr;
      continue;
    };

    // {n,m}
    if (expr[i] == '{'){
      int st = i+1;
      int en = -1;
      int comma = -1;
      bool nonGreedy = false;
      int j;
      for (j = i; j < expr.length(); j++){
        if (expr.length() > j+1 && expr[j] == '}' && expr[j+1] == '?'){
          en = j;
          nonGreedy = true;
          j++;
          break;
        };
        if (expr[j] == '}'){
          en = j;
          break;
        };
        if (expr[j] == ',') comma = j;
      };
      if (en == -1) return EBRACKETS;
      if (comma == -1) comma = en;
      next->s = UnicodeTools::getNumber(&DString(&expr, st, comma-st));
      if (comma != en) next->e = UnicodeTools::getNumber(&DString(&expr, comma+1, en-comma-1));
      else next->e = next->s;
      if (next->e == -1) return EOP;
      next->un.param = 0;
      if (en - comma == 1) next->e = -1;
      if (next->e == -1) next->op = nonGreedy ? ReNGRangeN : ReRangeN;
      else next->op = nonGreedy ? ReNGRangeNM : ReRangeNM;
      i = j;
      continue;
    };
    // ( ... )
    if (expr[i] == '('){
      bool namedBracket = false;
      // perl-like "uncaptured" brackets
      if (expr.length() >= i+2 && expr[i+1] == '?' && expr[i+2] == ':'){
        next->op = ReNamedBrackets;
        next->param0 = -1;
        namedBracket = true;
        i += 3;
      } else if (expr.length() > i+2 && expr[i+1] == '?' && expr[i+2] == '{'){
        // named bracket
        next->op = ReNamedBrackets;
        namedBracket = true;
        String *s_curly = UnicodeTools::getCurlyContent(expr, i+2);
        if (s_curly == null) return EBRACKETS;
        SString *br_name = new SString(s_curly);
        delete s_curly;
        int blen = br_name->length();
        if (blen == 0){
          next->param0 = -1;
          delete br_name;
        }else{
#ifndef NAMED_MATCHES_IN_HASH
#ifdef CHECKNAMES
          if (getBracketNo(br_name) != -1){
            delete br_name;
            return EBRACKETS;
          };
#endif
          if (cnMatch < NAMED_MATCHES_NUM){
            next->param0 = cnMatch;
            brnames[cnMatch] = br_name;
            cnMatch++;
          }else delete br_name;
#else
#ifdef CHECKNAMES
          if(br_name->length() && namedMatches && namedMatches->getItem(br_name)){
            delete br_name;
            return EBRACKETS;
          };
#endif
          next->param0 = 0;
          next->namedata = br_name;
          if (namedMatches){
            SMatch mt = {-1, -1};
            namedMatches->setItem(br_name, mt);
          };
#endif
        };
        i += blen+4;
      }else{
        next->op = ReBrackets;
        if (cMatch < MATCHES_NUM){
          next->param0 = cMatch;
          cMatch++;
        };
        i += 1;
      };
      next->un.param = new SRegInfo;
      next->un.param->parent = next;
      int endPos;
      EError err = setStructs(next->un.param, DString(&expr, i), endPos);
      if (endPos == expr.length()-i) return EBRACKETS;
      if (err) return err;
      i += endPos;
      continue;
    };

    // [] [^]
    if (expr[i] == '['){
      int endPos;
      CharacterClass *cc = CharacterClass::createCharClass(expr, i, &endPos);
      if (cc == null) return EENUM;
//      next->op = (exprn[i] == ReEnumS) ? ReEnum : ReNEnum;
      next->op = ReEnum;
      next->un.charclass = cc;
      i = endPos;
      continue;
    };
    if (expr[i] == ')' || expr[i] == ']' || expr[i] == '}') return EBRACKETS;
    next->op = ReSymb;
    next->un.symbol = expr[i];
  };

  // operators fixes
  for(next = re; next; next = next->next){
    // makes words from symbols
    SRegInfo *reword = next;
    SRegInfo *reafterword = next;
    SRegInfo *resymb;
    int wsize = 0;
    for (resymb = next;resymb && resymb->op == ReSymb; resymb = resymb->next, wsize++);
    if (resymb && resymb->op > ReBlockOps && resymb->op < ReSymbolOps){
      wsize--;
      resymb = resymb->prev;
    };
    if (wsize > 1){
      reafterword = resymb;
      resymb = reword;
      wchar *wcword = new wchar[wsize];
      for(int idx = 0; idx < wsize; idx++){
        wcword[idx] = resymb->un.symbol;
        SRegInfo *retmp = resymb;
        resymb = resymb->next;
        retmp->next = null;
        if (idx > 0) delete retmp;
      };
      reword->op = ReWord;
      reword->un.word = new SString(DString(wcword, 0, wsize));
      delete[] wcword;
      reword->next = reafterword;
      if (reafterword) reafterword->prev = reword;
      next = reword;
      continue;
    };

    // adds empty alternative
    while (next->op == ReOr){
      temp = new SRegInfo;
      temp->parent = next->parent;
      // |foo|bar
      if (!next->prev){
        temp->next = next;
        next->prev = temp;
        continue;
      };
      // foo||bar
      if (next->next && next->next->op == ReOr){
        temp->prev = next;
        temp->next = next->next;
        if (next->next) next->next->prev = temp;
        next->next = temp;
        continue;
      };
      // foo|bar|
      if (!next->next){
        temp->prev = next;
        temp->next = 0;
        next->next = temp;
        continue;
      };
      // foo|bar|*
      if (next->next->op > ReBlockOps && next->next->op < ReSymbolOps){
        temp->prev = next;
        temp->next = next->next;
        next->next->prev = temp;
        next->next = temp;
        continue;
      };
      delete temp;
      break;
    };
  };

  // op's generating...
  next = re;
  SRegInfo *realFirst;
  while(next){
    if (next->op > ReBlockOps && next->op < ReSymbolOps){
      if (!next->prev) return EOP;
      realFirst = next->prev;
      realFirst->next = 0;
      realFirst->parent = next;
      while(next->op == ReOr && realFirst->prev && realFirst->prev->op != ReOr){
        realFirst->parent = next;
        realFirst = realFirst->prev;
      };

      if (!realFirst->prev){
        re = next;
        next->un.param = realFirst;
        next->prev = 0;
      }else{
        next->un.param = realFirst;
        next->prev = realFirst->prev;
        realFirst->prev->next = next;
      };
      realFirst->prev = 0;
    };
    next = next->next;
  };
  if (retPos == -1) retPos = expr.length();
  return EOK;
};






////////////////////////////////////////////////////////////////////////////
// parsing
////////////////////////////////////////////////////////////////////////////

bool CRegExp::isWordBoundary(int &toParse)
{
  int before = 0;
  int after  = 0;
  if (toParse < end && (Character::isLetterOrDigit((*global_pattern)[toParse]) ||
      (*global_pattern)[toParse] == '_')) after = 1;
  if (toParse > 0 && (Character::isLetterOrDigit((*global_pattern)[toParse-1]) ||
      (*global_pattern)[toParse-1] == '_')) before = 1;
  return before+after == 1;
};
bool CRegExp::isNWordBoundary(int &toParse)
{
  return !isWordBoundary(toParse);
};




bool CRegExp::checkMetaSymbol(EMetaSymbols symb, int &toParse)
{
const String &pattern = *global_pattern;

  switch(symb){
    case ReAnyChr:
      if (toParse >= end) return false;
      if (!singleLine && (pattern[toParse] == 0x0A || pattern[toParse] == 0x0B ||
                          pattern[toParse] == 0x0C || pattern[toParse] == 0x0D ||
                          pattern[toParse] == 0x85 ||
                          pattern[toParse] == 0x2028 ||
                          pattern[toParse] == 0x2029)) return false;
      toParse++;
      return true;
    case ReSoL:
      if (multiLine){
        bool ok = false;
        if (toParse && (pattern[toParse-1] == 0x0A || pattern[toParse-1] == 0x0B ||
                          pattern[toParse-1] == 0x0C || pattern[toParse-1] == 0x0D ||
                          pattern[toParse-1] == 0x85 ||
                          pattern[toParse-1] == 0x2028 ||
                          pattern[toParse-1] == 0x2029)) ok = true;
        return (toParse == 0 || ok);
      };
      return (toParse == 0);
    case ReEoL:
      if (multiLine){
        bool ok = false; // ???check
        if (toParse && toParse < end &&
                         (pattern[toParse-1] == 0x0A || pattern[toParse-1] == 0x0B ||
                          pattern[toParse-1] == 0x0C || pattern[toParse-1] == 0x0D ||
                          pattern[toParse-1] == 0x85 ||
                          pattern[toParse-1] == 0x2028 ||
                          pattern[toParse-1] == 0x2029)) ok = true;
        return (toParse == end || ok);
      };
      return (end == toParse);
    case ReDigit:
      if (toParse >= end || !Character::isDigit(pattern[toParse])) return false;
      toParse++;
      return true;
    case ReNDigit:
      if (toParse >= end || Character::isDigit(pattern[toParse])) return false;
      toParse++;
      return true;
    case ReWordSymb:
      if (toParse >= end || !(Character::isLetterOrDigit(pattern[toParse])
          || pattern[toParse] == '_')) return false;
      toParse++;
      return true;
    case ReNWordSymb:
      if (toParse >= end || Character::isLetterOrDigit(pattern[toParse])
          || pattern[toParse] == '_') return false;
      toParse++;
      return true;
    case ReWSpace:
      if (toParse >= end || !Character::isWhitespace(pattern[toParse])) return false;
      toParse++;
      return true;
    case ReNWSpace:
      if (toParse >= end || Character::isWhitespace(pattern[toParse])) return false;
      toParse++;
      return true;
    case ReUCase:
      if (toParse >= end || !Character::isUpperCase(pattern[toParse])) return false;
      toParse++;
      return true;
    case ReNUCase:
      if (toParse >= end || !Character::isLowerCase(pattern[toParse])) return false;
      toParse++;
      return true;
    case ReWBound:
      return isWordBoundary(toParse);
    case ReNWBound:
      return isNWordBoundary(toParse);
    case RePreNW:
      if (toParse >= end) return true;
      return toParse == 0 || !Character::isLetter(pattern[toParse-1]);
#ifdef COLORERMODE
    case ReSoScheme:
      return (schemeStart == toParse);
    case ReStart:
      matches->s[0] = toParse;
      startChange = true;
      return true;
    case ReEnd:
      matches->e[0] = toParse;
      endChange = true;
      return true;
#endif
    default:
      return false;
  };
}

void CRegExp::check_stack(bool res,SRegInfo **re, SRegInfo **prev, int *toParse, bool *leftenter, int *action)
{
  if (stack==null){
    *action=res;
    return;
  }
  if (res/* && stack->ifTrueReturn<2*/){
    *action=stack->ifTrueReturn;
  }else{
    *action=stack->ifFalseReturn;
  }
  *re=stack->re;
  *prev=stack->prev;
  *toParse=stack->toParse;
  *leftenter=stack->leftenter;
  StackElem *t;
  t=stack;
  stack=t->prev_elem;
  delete t;
}

void CRegExp::insert_stack(SRegInfo **re, SRegInfo **prev, int *toParse, bool *leftenter, int ifTrueReturn, int ifFalseReturn, SRegInfo **re2, SRegInfo **prev2, int toParse2)
{
  StackElem *ne=new StackElem;
  ne->re=*re;
  ne->prev=*prev;
  ne->toParse=*toParse;
  ne->ifTrueReturn=ifTrueReturn;
  ne->ifFalseReturn=ifFalseReturn;
  ne->leftenter=*leftenter;
  ne->prev_elem=stack;
  stack=ne;

  *leftenter = true;
  if (prev2==null) *prev=null;
  else  *prev=*prev2;
  *re=*re2; 
  *toParse=toParse2;
  if (!*re){
    *re = (*prev)->parent;
    *leftenter = false;
  };

}

bool CRegExp::lowParse(SRegInfo *re, SRegInfo *prev, int toParse)
{
int i, sv, wlen;
bool leftenter = true;
bool br = false;
const String &pattern = *global_pattern;
int action=-1;

  if (!re){
    re = prev->parent;
    leftenter = false;
  };
  while (true){
    while(re || action!=-1){
      if (re && action==-1)
      switch(re->op){
      case ReEmpty:
        break;
      case ReBrackets:
      case ReNamedBrackets:
        if (leftenter){
          re->s = toParse;
          re = re->un.param;
          leftenter = true;
          continue;
        };
        if (re->param0 == -1) break;
        if (re->op == ReBrackets){
          if (re->param0 || !startChange)
            matches->s[re->param0] = re->s;
          if (re->param0 || !endChange)
            matches->e[re->param0] = toParse;
          if (matches->e[re->param0] < matches->s[re->param0])
            matches->s[re->param0] = matches->e[re->param0];
        }else{
#ifndef NAMED_MATCHES_IN_HASH
          matches->ns[re->param0] = re->s;
          matches->ne[re->param0] = toParse;
          if (matches->ne[re->param0] < matches->ns[re->param0])
            matches->ns[re->param0] = matches->ne[re->param0];
#else
          SMatch mt = { re->s, toParse };
          namedMatches->setItem(re->namedata, mt);
#endif
        };
        break;
      case ReSymb:
        if (toParse >= end){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }//return false;
        if (ignoreCase){
          if (Character::toLowerCase(pattern[toParse]) != Character::toLowerCase(re->un.symbol) &&
            Character::toUpperCase(pattern[toParse]) != Character::toUpperCase(re->un.symbol)){
              check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
              continue;
          }//  return false;
        }else if (pattern[toParse] != re->un.symbol){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }//return false;
        toParse++;
        break;
      case ReMetaSymb:
        if (!checkMetaSymbol(re->un.metaSymbol, toParse)){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }// return false;
        break;
      case ReWord:
        wlen = re->un.word->length();
        if (toParse+wlen > end) {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
          continue;
        }//return false;
        if (ignoreCase){
          if (!DString(&pattern, toParse, wlen).equalsIgnoreCase(re->un.word)){
            check_stack(false,&re,&prev,&toParse,&leftenter,&action);
            continue;
          }// return false;
          toParse += wlen;
        }else{
          br = false;
          for(i = 0; i < wlen; i++){
            if(pattern[toParse+i] != (*re->un.word)[i]){
              check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
              br = true;
              break;
            }// return false;
          };
          if (br) continue;
          toParse += wlen;
        }
        break;
      case ReEnum:
        if (toParse >= end){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }// return false;
        if (!re->un.charclass->inClass(pattern[toParse])) {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }//return false;
        toParse++;
        break;
      case ReNEnum:
        if (toParse >= end){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }// return false;
        if (re->un.charclass->inClass(pattern[toParse])){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
          continue;
        }// return false;
        toParse++;
        break;
#ifdef COLORERMODE
      case ReBkTrace:
        sv = re->param0;
        if (!backStr || !backTrace || sv == -1){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
          continue;
        }// return false;
        br = false;
        for (i = backTrace->s[sv]; i < backTrace->e[sv]; i++){
          if (toParse >= end || pattern[toParse] != (*backStr)[i]){
            check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
            br = true;
            break;
          }// return false;
          toParse++;
        };
        if (br) continue;
        break;
      case ReBkTraceN:
        sv = re->param0;
        if (!backStr || !backTrace || sv == -1){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
          continue;
        }// return false;
        br = false;
        for (i = backTrace->s[sv]; i < backTrace->e[sv]; i++){
          if (toParse >= end || Character::toLowerCase(pattern[toParse]) != Character::toLowerCase((*backStr)[i])) {
            check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
            br = true;
            break;
          }//return false;
          toParse++;
        };
        if (br) continue;
        break;
      case ReBkTraceName:
#ifndef NAMED_MATCHES_IN_HASH
        sv = re->param0;
        if (!backStr || !backTrace || sv == -1) {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }//return false;
        br = false;
        for (i = backTrace->ns[sv]; i < backTrace->ne[sv]; i++){
          if (toParse >= end || pattern[toParse] != (*backStr)[i]) {
            check_stack(false,&re,&prev,&toParse,&leftenter,&action);
            br = true;
            break;
          } //return false;
          toParse++;
        };
        if (br) continue;
        break;
#else
// !!!;
        {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }  //return false;
#endif // NAMED_MATCHES_IN_HASH
      case ReBkTraceNName:
#ifndef NAMED_MATCHES_IN_HASH
        sv = re->param0;
        if (!backStr || !backTrace || sv == -1) {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }// return false;
        br = false;
        for (i = backTrace->s[sv]; i < backTrace->e[sv]; i++){
          if (Character::toLowerCase(pattern[toParse]) != Character::toLowerCase((*backStr)[i]) || toParse >= end)  {
            check_stack(false,&re,&prev,&toParse,&leftenter,&action);
            br = true;
            break;
          } //return false;
          toParse++;
        };
        if (br) continue;
        break;
#else
// !!;
        {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }
        //return false;
#endif // NAMED_MATCHES_IN_HASH
#endif // COLORERMODE

      case ReBkBrackName:
#ifndef NAMED_MATCHES_IN_HASH
        sv = re->param0;
        if (sv == -1 || cnMatch <= sv) {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }//return false;
        if (matches->ns[sv] == -1 || matches->ne[sv] == -1) {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }//return false;
        br = false;
        for (i = matches->ns[sv]; i < matches->ne[sv]; i++){
          if (toParse >= end || pattern[toParse] != pattern[i]) {
            check_stack(false,&re,&prev,&toParse,&leftenter,&action);
            br = true;
            break;
          } //return false;
          toParse++;
        };
        if (br) continue;
        break;
#else
        {
          SMatch *mt = namedMatches->getItem(re->namedata);
          if (!mt) {
            check_stack(false,&re,&prev,&toParse,&leftenter,&action);
            continue;
          }//return false;
          if (mt->s == -1 || mt->e == -1) {
            check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
            continue;
          }// return false;
          br = false;
          for (i = mt->s; i < mt->e; i++){
            if (toParse >= end || pattern[toParse] != pattern[i]){
              check_stack(false,&re,&prev,&toParse,&leftenter,&action);
              br = true;
              break;
            } // return false;
            toParse++;
          };
          if (br) continue;
        };
        break;
#endif // NAMED_MATCHES_IN_HASH

      case ReBkBrack:
        sv = re->param0;
        if (sv == -1 || cMatch <= sv){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }// return false;
        if (matches->s[sv] == -1 || matches->e[sv] == -1){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }// return false;
        br = false;
        for (i = matches->s[sv]; i < matches->e[sv]; i++){
          if (toParse >= end || pattern[toParse] != pattern[i]){
            check_stack(false,&re,&prev,&toParse,&leftenter,&action);
            br = true;
            break;
          } // return false;
          toParse++;
        };
        if (br) continue;
        break;
      case ReAhead:
        if (!leftenter){
          check_stack(true,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }// return true;
        {
          insert_stack(&re,&prev,&toParse,&leftenter,10000,false,&re->un.param,0,toParse);
          continue;
        }//if (!lowParse(re->un.param, 0, toParse)) return false;
        break;
      case ReNAhead:
        if (!leftenter){
          check_stack(true,&re,&prev,&toParse,&leftenter,&action); 
          continue;
        }// return true;
        {
          insert_stack(&re,&prev,&toParse,&leftenter,false,10000,&re->un.param,0,toParse);
          continue;
        }//if (lowParse(re->un.param, 0, toParse)) return false;
        break;
      case ReBehind:
        if (!leftenter){
          check_stack(true,&re,&prev,&toParse,&leftenter,&action); 
          continue;
        }// return true;
        if (toParse - re->param0 < 0) {
          check_stack(false,&re,&prev,&toParse,&leftenter,&action); 
          continue;
        }
        else{
          insert_stack(&re,&prev,&toParse,&leftenter,10000,false,&re->un.param,0,toParse - re->param0);
          continue;
        }
        //if (toParse - re->param0 < 0 || !lowParse(re->un.param, 0, toParse - re->param0)) return false;
        break;
      case ReNBehind:
        if (!leftenter){
          check_stack(true,&re,&prev,&toParse,&leftenter,&action); 
          continue;
        }// return true;
        if (toParse - re->param0 >= 0){ 
          insert_stack(&re,&prev,&toParse,&leftenter,false,10000,&re->un.param,0,toParse - re->param0);
          continue;
        }
        //if (toParse - re->param0 >= 0 && lowParse(re->un.param, 0, toParse - re->param0)) return false;
        break;

      case ReOr:
        if (!leftenter){
          while (re->next)
            re = re->next;
          break;
        };
        {
          insert_stack(&re,&prev,&toParse,&leftenter,true,10000,&re->un.param,0,toParse );
          continue;
        }// if (lowParse(re->un.param, 0, toParse)) return true;
        break;
      case ReRangeN:
        // first enter into op
        if (leftenter){
          re->param0 = re->s;
          re->oldParse = -1;
        };
        if (!re->param0 && re->oldParse == toParse) break;
        re->oldParse = toParse;
        // making branch
        if (!re->param0){
          insert_stack(&re,&prev,&toParse,&leftenter,true,10001,&re->un.param,0,toParse );
          continue;
          //if (lowParse(re->un.param, 0, toParse))
          //  return true;
          //return lowParse(re->next, re, toParse);
        };
        // go into
        if (re->param0) re->param0--;
        re = re->un.param;
        leftenter = true;
        continue;
      case ReRangeNM:
        if (leftenter){
          re->param0 = re->s;
          re->param1 = re->e - re->s;
          re->oldParse = -1;
        };
        //
        if (!re->param0){
          if (re->param1) re->param1--;
          else{
            insert_stack(&re,&prev,&toParse,&leftenter,true,false,&re->next,&re,toParse );
            continue;
          }//return lowParse(re->next, re, toParse);
          {
            insert_stack(&re,&prev,&toParse,&leftenter,true,10002,&re->un.param,0,toParse );
            continue;
          }
        /*  if (lowParse(re->un.param, 0, toParse)) return true;
          if (lowParse(re->next, re, toParse)) return true;
          re->param1++;
          return false;*/
        };
        if (re->param0) re->param0--;
        re = re->un.param;
        leftenter = true;
        continue;
      case ReNGRangeN:
        if (leftenter){
          re->param0 = re->s;
          re->oldParse = -1;
        };
        if (!re->param0 && re->oldParse == toParse) break;
        re->oldParse = toParse;
        //
        if (!re->param0){
          insert_stack(&re,&prev,&toParse,&leftenter,true,10004,&re->next,&re,toParse );
          continue;
        }
        //if (!re->param0 && lowParse(re->next, re, toParse)) return true;
        if (re->param0) re->param0--;
        re = re->un.param;
        leftenter = true;
        continue;
      case ReNGRangeNM:
        if (leftenter){
          re->param0 = re->s;
          re->param1 = re->e - re->s;
          re->oldParse = -1;
        };
        //
        if (!re->param0){
          if (re->param1) re->param1--;
          else {
            insert_stack(&re,&prev,&toParse,&leftenter,true,false,&re->next,&re,toParse );
            continue;
            //return lowParse(re->next, re, toParse);
          }
          {
            insert_stack(&re,&prev,&toParse,&leftenter,true,10005,&re->next,&re,toParse );
            continue;
          }//if (lowParse(re->next, re, toParse)) return true;
        /*  if (lowParse(re->un.param, 0, toParse))
            return true;
          else{
            re->param1++;
            return false;
          };*/
        };
        if (re->param0) re->param0--;
        re = re->un.param;
        leftenter = true;
        continue;
    };
   
    switch (action){
      case 0: 
        if (stack){
          check_stack(false,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }else
          return 0; 
        break;
      case 1: 
        if (stack){
          check_stack(true,&re,&prev,&toParse,&leftenter,&action);
          continue;
        }else
          return 1; 
        break;
      case 10000: 
        action = -1; 
        break;
      case 10001: 
        action = -1;
        insert_stack(&re,&prev,&toParse,&leftenter,true,false,&re->next,&re,toParse);
        //return lowParse(re->next, re, toParse);
        continue;
        break;
      case 10002:
        action = -1;
        insert_stack(&re,&prev,&toParse,&leftenter,true,10003,&re->next,&re,toParse);
        continue;
        //if (lowParse(re->next, re, toParse)) return true;
        //re->param1++;
        //return false;
        break;
      case 10003:
        action = -1;
        re->param1++;
        check_stack(false,&re,&prev,&toParse,&leftenter,&action);
        continue;
         //re->param1++;
        //return false;
        break;
      case 10004:
        action = -1;
        if (re->param0) re->param0--;
        re = re->un.param;
        leftenter = true;
        continue;
        break;
      case 10005:
        action = -1;
        insert_stack(&re,&prev,&toParse,&leftenter,true,10006,&re->un.param,0,toParse);
        continue;

        //if (lowParse(re->un.param, 0, toParse))
        //  return true;
        //else{
        //  re->param1++;
        //  return false;
        //};
        break;
      case 10006:
        action = -1;
        re->param1++;
        check_stack(false,&re,&prev,&toParse,&leftenter,&action);
        continue;
        //re->param1++;
        //return false;
        break;
    }
    if (!re->next){
      re = re->parent;
      leftenter = false;
    }else{
      re = re->next;
      leftenter = true;
    };
  };
  check_stack(true,&re,&prev,&toParse,&leftenter,&action);
  }
};

inline bool CRegExp::quickCheck(int toParse)
{
  if (firstChar != BAD_WCHAR){
    if (toParse >= end) return false;
    if (ignoreCase){
      if (Character::toLowerCase((*global_pattern)[toParse]) != Character::toLowerCase(firstChar)) return false;
    }else
      if ((*global_pattern)[toParse] != firstChar) return false;
    return true;
  };
  if (firstMetaChar != ReBadMeta)
  switch(firstMetaChar){
    case ReSoL:
      if (toParse != 0) return false;
      return true;
#ifdef COLORERMODE
    case ReSoScheme:
      if (toParse != schemeStart) return false;
      return true;
#endif
//    case ReWBound:
//      return relocale->cl_isword(*toParse) && (toParse == start || !relocale->cl_isword(*(toParse-1)));
  };
  return true;
};

inline bool CRegExp::parseRE(int pos)
{
  if (error) return false;

  int toParse = pos;

  if (!positionMoves && (firstChar != BAD_WCHAR || firstMetaChar != ReBadMeta) && !quickCheck(toParse))
    return false;

  int i;
  for (i = 0; i < cMatch; i++)
    matches->s[i] = matches->e[i] = -1;
  matches->cMatch = cMatch;
#ifndef NAMED_MATCHES_IN_HASH
  for (i = 0; i < cnMatch; i++)
    matches->ns[i] = matches->ne[i] = -1;
  matches->cnMatch = cnMatch;
#endif
  do{
    stack=null;
    if (lowParse(tree_root, 0, toParse)) return true;
    if (!positionMoves) return false;
    toParse = ++pos;
  }while(toParse <= end);
  return false;
};

bool CRegExp::parse(const String *str, int pos, int eol, SMatches *mtch
#ifdef NAMED_MATCHES_IN_HASH
, PMatchHash nmtch
#endif
, int soScheme, int posMoves)
{
  bool nms = positionMoves;
  if (posMoves != -1) positionMoves = (posMoves != 0);
#ifdef COLORERMODE
  schemeStart = soScheme;
#endif
  global_pattern = str;
  end = eol;
  matches = mtch;
#ifdef NAMED_MATCHES_IN_HASH
  namedMatches = nmtch;
#endif
  bool result = parseRE(pos);
  positionMoves = nms;
  return result;
};

bool CRegExp::parse(const String *str, SMatches *mtch
#ifdef NAMED_MATCHES_IN_HASH
,PMatchHash nmtch
#endif
)
{
  end = str->length();
  global_pattern = str;
#ifdef COLORERMODE
  schemeStart = 0;
#endif
  matches = mtch;
#ifdef NAMED_MATCHES_IN_HASH
  namedMatches = nmtch;
#endif
  return parseRE(0);
};

/////////////////////////////////////////////////////////////////
// other methods

bool CRegExp::setRE(const String *re)
{
  error = EERROR;
#ifdef NAMED_MATCHES_IN_HASH
  PMatchHash oldnamedMatches = namedMatches;
  SMatchHash tmpMatchHash;
  namedMatches = &tmpMatchHash;
  error = setRELow(*re);
  namedMatches = oldnamedMatches;
#else
  error = setRELow(*re);
#endif
  return error == EOK;
};
bool CRegExp::isOk()
{
  return error == EOK;
};
EError CRegExp::getError()
{
  return error;
};

bool CRegExp::setPositionMoves(bool moves)
{
  positionMoves = moves;
  return true;
};

#ifndef NAMED_MATCHES_IN_HASH
int CRegExp::getBracketNo(const String *brname)
{
  for(int brn = 0; brn < cnMatch; brn++)
    if (brname->equalsIgnoreCase(brnames[brn])) return brn;
  return -1;
};
String *CRegExp::getBracketName(int no)
{
  if (no >= cnMatch) return 0;
  return brnames[no];
};
#endif

#ifdef COLORERMODE
bool CRegExp::setBackRE(CRegExp *bkre)
{
  this->backRE = bkre;
  return true;
};
bool CRegExp::setBackTrace(const String *str, SMatches *trace)
{
  backTrace = trace;
  backStr = str;
  return true;
};
bool CRegExp::getBackTrace(const String **str, SMatches **trace)
{
  *str = backStr;
  *trace = backTrace;
  return true;
};
#endif
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Colorer Library.
 *
 * The Initial Developer of the Original Code is
 * Cail Lomecb <cail@nm.ru>.
 * Portions created by the Initial Developer are Copyright (C) 1999-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
