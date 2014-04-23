#include "nd.h"
#include "tictoc.h"
#include <stdlib.h> // for rand
#include <stdio.h>  // for printf
#include <string.h> // for argument parsing

#define LOG(...)     printf(__VA_ARGS__)
#define REPORT(estr) LOG("%s(%d): %s()\n\t%s\n\tEvaluated to false.\n",__FILE__,__LINE__,__FUNCTION__,estr)
#define TRY(e)       do{if(!(e)){REPORT(#e);goto Error;}}while(0)

#define ITER (10000)

char *basename(char *path)
{
  char *base = strrchr(path, '/');   //try posix path sep 1st
  base=base?base:strrchr(path,'\\'); //windows path sep 2nd
  return base ? base+1 : path;
}
void usage(const char* basename)
{ printf("\n\tUsage: %s [random|sequence]\n\n",basename);
}

nd_t fill(nd_t a)
{ unsigned short *d=(unsigned short*)nddata(a);
  unsigned i;
  for(i=0;i<ndnelem(a);++i)
    d[i]=(unsigned short)rand();
  return a;
}

typedef size_t* (*stepper_t)(size_t *origin, int max );
size_t* step_random(size_t *origin, int max )
{ origin[2]=rand()%max;
  return origin;
}
size_t* step_sequential(size_t *origin, int max )
{ origin[2]=(origin[2]+1)%max;
  return origin;
}

stepper_t argparse(int argc,char*argv[])
{ if(argc!=2) goto Error;
  if(strcmp(argv[1],"random"  )==0) return step_random;
  if(strcmp(argv[1],"sequence")==0) return step_sequential;
Error:
  usage(basename(argv[0]));
  return 0;
}

int main(int argc, char* argv[])
{ int  eflag=0;
  nd_t shape=0,
           a=0,
       times=0;
  ndio_t   f=0;
  stepper_t step=0;
  TRY(step=argparse(argc,argv));

  TRY(ndcast(ndreshapev(shape=ndinit(),1,ITER),nd_f64));
  TRY(times=ndheap(shape));
  ndfree(shape);
  shape=0;

  ndioClose(f=ndioOpen("test.mp4",NULL,"r")); 
  if(!f) // if the file exists and is readible, skip generation
  { TRY(ndcast(ndreshapev(shape=ndinit(),3,512,512,512),nd_u16));
    TRY(a=fill(ndheap(shape)));
    ndioClose(ndioWrite(f=ndioOpen("test.mp4",NULL,"w"),a));
    TRY(f);
    ndfree(a);
    ndfree(shape);
    a=shape=0;
  }
  f=0;

  {
    size_t i,ndim,origin[8]={0},max; // max 8 dims
    double *t=nddata(times);

    TRY(f=ndioOpen("test.mp4",NULL,"r"));
    shape=ndioShape(f);
    ndim=ndndim(shape);
    max=ndshape(shape)[2]; // should be 512
    ndshape(shape)[2]=1;   // one plane of the input
    TRY(a=ndheap(shape));

    TRY(ndioReadSubarray(f,a,step(origin,max),NULL));
    for(i=0;i<ITER;++i)
    { tic();
      TRY(ndioReadSubarray(f,a,step(origin,max),NULL));
      t[i]=toc(NULL);
    }
    ndioClose(ndioWrite(ndioOpen("random-seeks.h5",NULL,"w"),times));
  }

Finalize:
  ndioClose(f);
  ndfree(times);
  ndfree(a);
  ndfree(shape);
  return eflag;
Error:
  eflag=1;
  goto Finalize;
}
