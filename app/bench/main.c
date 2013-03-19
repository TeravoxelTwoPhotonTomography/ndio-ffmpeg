#include "nd.h"
#include "tictoc.h"
#include <stdlib.h> // for rand
#include <stdio.h>  // for printf

#define LOG(...)     printf(__VA_ARGS__)
#define REPORT(estr) LOG("%s(%d): %s()\n\t%s\n\tEvaluated to false.\n",__FILE__,__LINE__,__FUNCTION__,estr)
#define TRY(e)       do{if(!(e)){REPORT(#e);goto Error;}}while(0)

#define ITER (10000)

nd_t fill(nd_t a)
{ unsigned short *d=(unsigned short*)nddata(a);
  unsigned i;
  for(i=0;i<ndnelem(a);++i)
    d[i]=(unsigned short)rand();
  return a;
}

size_t* step(size_t *origin, int max )
{ origin[2]=rand()%max;
  return origin;
}


int main(int argc, char* argv[])
{ int  eflag=0;
  nd_t shape=0,
           a=0,
       times=0;    
  ndio_t   f=0;
  ndioClose(f=ndioOpen("test.mp4",NULL,"r")); 
  if(!f) // if the file exists and is readible, skip generation
  { TRY(ndcast(ndreshapev(ndinit(),3,512,512,512),nd_u16));
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
    TRY(ndcast(ndreshapev(ndinit(),1,ITER),nd_f64));
    TRY(ndref(times=ndinit(),malloc(ITER*sizeof(double)),nd_heap));

    TRY(f=ndioOpen("test.mp4",NULL,"r"));
    shape=ndioShape(f);
    ndim=ndndim(shape);
    max=ndshape(shape)[2]; // should be 512
    shape=ndreshape(shape,2,ndshape(shape)); // one plane of the input
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