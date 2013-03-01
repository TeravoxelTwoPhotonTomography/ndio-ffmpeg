/**
 * \file
 * Implementation of the BSD function: strsep().
 */

/**
 * Implementation of the BSD function: strsep().
 * strsep() is a bit more robust than the more portable strtok().
 */
char* strsep(char **stringp, const char *delim)
{ char *s,*tok;
  if(!(s=*stringp)) return 0;
  for(tok=s;;)
  { int c, sc;
    const char *spanp;
    c=*s++;
    spanp=delim;
    do {
      if((sc=*spanp++)==c)
      { if(c==0) s=0;
        else     s[-1]=0;
        *stringp=s;
        return (tok);
      }
    } while(sc!=0);
  }  
  return 0; /* NOTREACHED */
}