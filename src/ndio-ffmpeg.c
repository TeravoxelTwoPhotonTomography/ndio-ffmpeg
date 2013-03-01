/** \file
    Plugin interface to FFMPEG.

    \section ndio-ffmpeg-read

    Two options for data flow:
    1. Try to decode directly into destination array
    2. Decode frame into temporary buffer and then copy to destination array.

    Option (2) is simpler to code.  I think you have to override some
    (get|release)_buffer functions somewhere in order to do option 1.  Another
    advantage of option 2 is that we get to choose how to translate strange
    pixel formats.

    \author Nathan Clack
    \date   June 2012

    Writer
    -----
    \todo Apples VPXDecode service crashes due to my h264 encoded videos...not sure what's
          wrong. Transcoding with ffmpeg cleans them up, so it's certainly my code.
    \todo Channel ordering on read is different than channel ordering required for write, which
          is awkward.
    \todo convert more formats to a writable form.  Favor writing something approximate over not
          writing anything at all.
    \todo probabaly have to do a copy for some conversions.
    \todo handle options

    Reader
    ------
    \todo what to do with multiple video streams?
    \todo Channel ordering on read is different than channel ordering required for write, which
          is awkward.
    \todo for appropriate 1d data, use audio streams
    \todo Correct format translation for PAL.

    \section ndio-ffmpeg-notes Notes

        * duration be crazy
          * mostly because durations are stored with different time bases in different places
          * self->fmt->duration
            * seems like it's always set (not NOPTS)
            * self->fmt->duration/AV_TIME_BASE -> duration in seconds
          * self->fmt->streams[0]->duration
            * sometimes not set (NOPTS)
            * for mpeg and ogg, looks like this is just the number of frames
          * the right thing to do looks like
            1. get duration in secodns from format context
            2. convert to frames using stream frame rate.
               * stream->r_frame_rate looks more reliable than time_base
                 * different than 1/time_base for webm/v8

        * FFMPEG API documentation has lots of examples, but it's hard to know which one's to
          use.  Some of the current examples use depricated APIs.
          * The process of read/writing a container file is called demuxing/muxing.
          * The process of unpacking/packing a video stream is called decoding/encoding.
*/
#include "strsep.h"
#include "nd.h"
#include "src/io/interface.h"
#include <stdint.h>
#include <string.h>

// need to define inline before including av* headers on C89 compilers
#ifdef _MSC_VER
#define inline __forceinline
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"

/// @cond DEFINES
#define countof(e)        (sizeof(e)/sizeof(*e))
#define ENDL              "\n"
#define LOG(...)          fprintf(stderr,__VA_ARGS__)
#define TRY(e)            do{if(!(e)) { LOG("%s(%d): %s"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)
#define NEW(type,e,nelem) TRY((e)=(type*)malloc(sizeof(type)*(nelem)))
#define SAFEFREE(e)       if(e){free(e); (e)=NULL;}
#define FAIL(msg)         do{ LOG("%s(%d): %s"ENDL "\tExecution should not have reached this point."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,msg); goto Error; }while(0)
#define AVTRY(expr,msg) \
  do{                                                       \
    int v=(expr);                                           \
    if(v<0 && v!=AVERROR_EOF)                               \
    { char buf[1024];                                       \
      av_strerror(v,buf,sizeof(buf));                       \
      LOG("%s(%d):"ENDL "\t%s"ENDL "\t%s"ENDL "\tFFMPEG: %s"ENDL, \
          __FILE__,__LINE__,#expr,(char*)msg,buf);          \
      goto Error;                                           \
    }                                                       \
  }while(0)

static const AVRational ONE  = {1,1};
static const AVRational FREQ = {1,AV_TIME_BASE}; // same as AV_TIME_BASE_Q, but more MSVC friendly
#define STREAM(e)   ((e)->fmt->streams[(e)->istream])
#define CCTX(e)     ((e)->fmt->streams[(e)->istream]->codec)    ///< gets the AVCodecContext for the selected video stream
#define DURATION(e) (av_rescale_q((e)->fmt->duration,av_mul_q(FREQ,STREAM(e)->r_frame_rate),ONE)) ///< gets the duration in #frames
/// @endcond

static int is_one_time_inited = 0; /// Tracks whether avcodec has been init'd.  \todo should be mutexed

/** File context used for operating on video files with FFMPEG */
typedef struct _ndio_ffmpeg_t
{ AVFormatContext   *fmt;     ///< The main handle to the open file
  struct SwsContext *sws;     ///< Software scaling context
  AVFrame           *raw;     ///< The frame buffer for holding data before translating it to the output nd_t format
  int                istream; ///< The stream index.
  int64_t            nframes; ///< Duration of video in frames (for reading)
  int64_t            iframe;  ///< the last requested frame (for seeking)
  AVDictionary      *opts;    ///< (unused at the moment)
} *ndio_ffmpeg_t;

//
//  === HELPERS ===
//

/** returns x if x is even, otherwise x+1 */
int even(int x) { if (x%2) return x+1; return x; }

/** One-time initialization for ffmpeg library.

    This gets called by ndio_get_format_api(), so it's guaranteed to be called
    before any of the interface implementation functions.
 */
static void maybe_init()
{ if(is_one_time_inited)
    return;
  avcodec_register_all();
  av_register_all();
  avformat_network_init();
  is_one_time_inited = 1;

  av_log_set_level(0
#if 0
    |AV_LOG_DEBUG
    |AV_LOG_VERBOSE
    |AV_LOG_INFO
    |AV_LOG_WARNING
    |AV_LOG_ERROR
    |AV_LOG_FATAL
    |AV_LOG_PANIC
#endif
    );
}

/** FFMPEG to nd_t type conversion */
int pixfmt_to_nd_type(int pxfmt, nd_type_id_t *type, int *nchan)
{ int ncomponents   =av_pix_fmt_descriptors[pxfmt].nb_components;
  int bits_per_pixel=av_get_bits_per_pixel(av_pix_fmt_descriptors+pxfmt);
  int nbytes;
  TRY(ncomponents>0);
  nbytes=(int)ceil(bits_per_pixel/ncomponents/8.0f);
  *nchan=ncomponents;
  switch(nbytes)
  { case 1: *type=nd_u8;  return 1;
    case 2: *type=nd_u16; return 1;
    case 4: *type=nd_u32; return 1;
    case 8: *type=nd_u64; return 1;
    default:
      ;
  }
Error:
  return 0;
}

/** Recommend pixel type based on nd_t type attributes. */
enum PixelFormat to_pixfmt(int nbytes, int ncomponents)
{
  switch(ncomponents)
  { case 1:
      switch(nbytes)
      { case 1: return PIX_FMT_GRAY8;
        case 2: return PIX_FMT_GRAY16;
        default:;
      }
      break;
    case 2: // for write
    case 3:
      switch(nbytes)
      { case 1: return PIX_FMT_RGB24;
        case 2: return PIX_FMT_RGB48;
        default:;
      }
      break;
    case 4:
      switch(nbytes)
      { case 1: return PIX_FMT_RGBA;
        case 2: return PIX_FMT_RGBA64;
        default:;
      }
      break;
    default:
      ;
  }
  return PIX_FMT_NONE;
}

/** Recommend output pixel format based on intermediate pixel format. */
enum PixelFormat pixfmt_to_output_pixfmt(int pxfmt)
{ uint8_t flags = av_pix_fmt_descriptors[pxfmt].flags;
  int ncomponents   =av_pix_fmt_descriptors[pxfmt].nb_components;
  int bits_per_pixel=av_get_bits_per_pixel(av_pix_fmt_descriptors+pxfmt);
  int bytes=ncomponents?(int)ceil(bits_per_pixel/ncomponents/8.0f):0;
  //if(flags&(PIX_FMT_PAL|PIX_FMT_PSEUDOPAL))
  //  return PIX_FMT_RGB;
  return to_pixfmt(bytes,ncomponents);
}


//
//  === INTERFACE ===
//

/** Returns the plugin name. */
static const char* name_ffmpeg(void) { return "ffmpeg"; }

/** \returns 1 if strings are the same, otherwise 0 */
static unsigned streq(const char* a, const char* b)
{ if(strlen(a)!=strlen(b)) return 0;
  return strcmp(a,b)==0;
}

/** \returns 1 if \a is an exact match for a word in a comma-seperated list of words. */
static unsigned in(const char* a, const char* list)
{ unsigned out=0;
  char *token,*bookmark,*copy;
  TRY(copy=bookmark=strdup(list));
  while(!out && (token=strsep(&bookmark,","))!=NULL)
    out|=streq(a,token);
Finalize:
  free(copy);
  return out;
Error:
  out=0;
  goto Finalize;
}

/** 
 * Just does matching of the extension to a format shortname registered with ffmpeg.
 * \returns true if the file is readible using this interface. 
 */
static unsigned test_readable(const char *path)
{ 
#if 1  
  AVInputFormat *fmt=0;
  const char *ext;
  ext=(ext=strrchr(path,'.'))?(ext+1):""; // yields something like "mp4" or, if no extension found, "".
  while( fmt=av_iformat_next(fmt) )
  { if(in(ext,fmt->name))
      return 1;
  }
  return fmt!=0;
#endif
#if 0  // This approach tries to open the file.  Too expensive. 
  AVFormatContext *fmt=0;
  // just check that container can be opened; don't worry about streams, etc...
  if(0==avformat_open_input(&fmt,path,NULL/*input format*/,NULL/*options*/))
  {
    int ok=(0<=avformat_find_stream_info(fmt,NULL));
    if(ok) //check the codec
    { AVCodec *codec=0;
      AVCodecContext *cctx=0;
      int i=av_find_best_stream(fmt,AVMEDIA_TYPE_VIDEO,-1,-1,&codec,0/*flags*/);
      if(!codec || codec->id==CODEC_ID_TIFF) //exclude tiffs because ffmpeg can't properly do multiplane tiff
        ok=0;
    }
    avformat_close_input(&fmt);
    return ok;
  }
  return 0;
#endif
}

/** \returns true if the file is writable using this interface. */
static unsigned test_writable(const char *path)
{ AVOutputFormat *fmt=0;
  const char *ext;
  ext=(ext=strrchr(path,'.'))?(ext+1):""; // yields something like "mp4" or, if no extension found, "".
  while( fmt=av_oformat_next(fmt) )
  { //LOG("%s\n",fmt->name);
    if(in(ext,fmt->name))
      return fmt->video_codec!=CODEC_ID_NONE;
  }
  return 0;
}

/** \returns true if the plugin may be used with the file at \a path according to the \a mode. */
static unsigned is_ffmpeg(const char *path, const char *mode)
{
  switch(mode[0])
  { case 'r': return test_readable(path);
    case 'w': return test_writable(path);
    default:
      ;
  }
  return 0;
}

/** Opens the file at \a path for reading */
static ndio_ffmpeg_t open_reader(const char* path)
{ ndio_ffmpeg_t self=0;
  NEW(struct _ndio_ffmpeg_t,self,1);
  memset(self,0,sizeof(*self));
  self->iframe=-1;

  TRY(self->raw=avcodec_alloc_frame());
  AVTRY(avformat_open_input(&self->fmt,path,NULL/*input format*/,NULL/*options*/),path);
  AVTRY(avformat_find_stream_info(self->fmt,NULL),"Failed to find stream information.");
  { AVCodec        *codec=0;
    AVCodecContext *cctx=CCTX(self);
    AVTRY(self->istream=av_find_best_stream(self->fmt,AVMEDIA_TYPE_VIDEO,-1,-1,&codec,0/*flags*/),"Failed to find a video stream.");
    AVTRY(avcodec_open2(cctx,codec,NULL/*options*/),"Cannot open video decoder."); // inits the selected stream's codec context

    TRY(self->sws=sws_getContext(cctx->width,cctx->height,cctx->pix_fmt,
                                  cctx->width,cctx->height,pixfmt_to_output_pixfmt(cctx->pix_fmt),
                                  SWS_BICUBIC,NULL,NULL,NULL));

    self->nframes  = DURATION(self);
  }
  return self;
Error:
  if(self)
  { if(CCTX(self)) avcodec_close(CCTX(self));
    if(self->opts) av_dict_free(&self->opts);
    if(self->raw)  av_free(self->raw);
    if(self->sws)  sws_freeContext(self->sws);
    if(self->fmt)  avformat_free_context(self->fmt);
    free(self);
  }
  return NULL;
}

/** Opens the file at \a path for writing */
static ndio_ffmpeg_t open_writer(const char* path)
{ ndio_ffmpeg_t self=0;
  AVCodec *codec;
  NEW(struct _ndio_ffmpeg_t,self,1);
  memset(self,0,sizeof(*self));
  TRY(self->raw=avcodec_alloc_frame());

  AVTRY(avformat_alloc_output_context2(&self->fmt,NULL,NULL,path), "Failed to detect output file format from the file name.");
  TRY(self->fmt->oformat && self->fmt->oformat->video_codec!=CODEC_ID_NONE); //Assert that this is a video output format
  TRY(codec=avcodec_find_encoder(self->fmt->oformat->video_codec));
  TRY(avformat_new_stream(self->fmt,codec)); //returns the stream, assume it's index 0 from here on out (self->istream=0 is accurate)
  // maybe open the output file
  TRY(0==(self->fmt->flags&AVFMT_NOFILE));                              //if the flag is set, don't need to open the file (I think).  Assert here so I get notified of when this happens.  Expected to be rare/never.
  AVTRY(avio_open(&self->fmt->pb,path,AVIO_FLAG_WRITE),"Failed to open output file.");
  CCTX(self)->codec=codec;
  AVTRY(CCTX(self)->pix_fmt=codec->pix_fmts[0],"Codec indicates that no pixel formats are supported.");  
  return self;
Error:
  if(self->fmt->pb) avio_close(self->fmt->pb);
  if(self->opts) av_dict_free(&self->opts);
  if(self)
  { if(self->fmt)  avformat_free_context(self->fmt);
    free(self);
  }
  return NULL;
}

/** Encodes the input frame, outputing any resulting packets to the output stream.
 *
 *  \param[in]      file    Output file context.  Passed in for logging purposes.
 *  \param[in]      fmt     The output format context.
 *  \param[in]      cctx    The encoding context.
 *  \param[in]      stream  The stream context.
 *  \param[in]      packet  Address of init'd (maybe pre-allocated) packet.
 *  \param[in]      frame   The video frame to encode.
 *  \param[out] got_packet  1 if encoding yielded a packet, 0 otherwise.
 */
static int push(ndio_t file, AVPacket *p, AVFrame *frame, int *got_packet)
{ ndio_ffmpeg_t self=(ndio_ffmpeg_t)ndioContext(file);
  AVFormatContext *fmt=self->fmt;
  AVCodecContext *cctx=CCTX(self);
  AVStream     *stream=STREAM(self);
  *got_packet=0;
  AVTRY(avcodec_encode_video2(cctx,p,frame,got_packet), frame?"Failed to encode frame.":"Failed to encode terminating frame.");
  if(*got_packet)
  { if (p->pts != AV_NOPTS_VALUE)
      p->pts = av_rescale_q(p->pts, cctx->time_base, stream->time_base);
    if (p->dts != AV_NOPTS_VALUE)
      p->dts = av_rescale_q(p->dts, cctx->time_base, stream->time_base);
    AVTRY(av_write_frame(fmt,p),"Failed to write packet.");
    self->nframes++; // at the moment, mostly just use this to record that we did write something.
    av_destruct_packet(p);
  }
  return 1;
Error:
  return 0;
}

/** Flushes frames, writes the footer and closes the output file. */
static int close_writer(ndio_t file)
{ ndio_ffmpeg_t self;
  TRY(self=(ndio_ffmpeg_t)ndioContext(file));
  TRY(CCTX(self)->codec); // codec might not have been opened
  if(self->nframes)
  { if(CCTX(self)->codec->capabilities & CODEC_CAP_DELAY)
    { AVPacket p={0};
      int got_packet=1;
      av_init_packet(&p);
      while(got_packet)
        TRY(push(file,&p,NULL,&got_packet));
    }
    AVTRY(av_write_trailer(self->fmt),"Failed to write trailer.");
  }
  return 1;
Error:
  if(self->fmt->pb) avio_close(self->fmt->pb);
  return 0;
}

/** Intializes the codec context if necessary. */
static int maybe_init_codec_ctx(ndio_ffmpeg_t self, int width, int height, int fps, int src_pixfmt)
{ AVCodecContext *cctx=CCTX(self);
  AVCodec *codec=cctx->codec;
  if(!cctx->width)
  { cctx->width =even(width);
    cctx->height=even(height);
    cctx->time_base.num=1;
    cctx->time_base.den=fps;
    cctx->gop_size=12;
    AVTRY(avcodec_open2(cctx,codec,&self->opts),"Failed to initialize encoder.");

    TRY(self->sws=sws_getContext(
      width,height,src_pixfmt,
      even(width),even(height),cctx->pix_fmt,
      SWS_BICUBIC,NULL,NULL,NULL));

    TRY(av_image_alloc(self->raw->data,self->raw->linesize,even(width),even(height),cctx->pix_fmt,1));
    AVTRY(avformat_write_header(self->fmt,&self->opts),"Failed to write header.");
  }
  return 1;
Error:
  return 0;
}

/** Opens the file at \a path according to the specified \a mode. */
static void* open_ffmpeg(const char* path, const char *mode)
{
  switch(mode[0])
  { case 'r': return open_reader(path);
    case 'w': return open_writer(path);
    default:
      FAIL("Could not recognize mode.");
  }
Error:
  return 0;
}

// Following functions will log to the file object.
/// @cond DEFINES
#undef  LOG
#define LOG(...) ndioLogError(file,__VA_ARGS__)
/// @endcond

/** Closes the file and performs any necessary cleanup */
static void close_ffmpeg(ndio_t file)
{ ndio_ffmpeg_t self;
  if(!file) return;
  if(!(self=(ndio_ffmpeg_t)ndioContext(file)) ) return;
  if(self->fmt)
  { if(self->fmt->oformat)
      close_writer(file);
    if(CCTX(self))  avcodec_close(CCTX(self));
    avformat_free_context(self->fmt);
  }
  if(self->opts)    av_dict_free(&self->opts);
  if(self->raw)     av_free(self->raw);
  if(self->sws)     sws_freeContext(self->sws);
  free(self);
}

/** Packs a shape array, removing dimensions of size 1.
 *  \returns the number of dimensions after packing.
 */
static int pack(size_t *s, int n)
{ int i,c;
  for(i=0,c=0;i<n;++i)
  { s[c]=s[i];
    if(s[i]!=1) ++c;
  }
  return c;
}

/** Prefix scan with multiplication as the operator. */
static size_t prod(const size_t *s, size_t n)
{ size_t i,p=1;
  for(i=0;i<n;++i) p*=s[i];
  return p;
}

/**Computes the shape of the array in \a file.
 * \returns an nd_t with the shape, stride, and type that will be read from file.
 *          The array references no data; That is \code nddata(a)==NULL \endcode
 * \todo FIXME: Must decode first frame to ensure codec context has the proper width.
 * \todo FIXME: side effect, seeks to begining of video.  Should leave current seek
 *              point unmodified.
 */
static nd_t shape_ffmpeg(ndio_t file)
{ int w,h,d,c;
  nd_type_id_t type;
  ndio_ffmpeg_t self;
  AVCodecContext *cctx;
  AVPacket packet={0};
  TRY(file);
  TRY(self=(ndio_ffmpeg_t)ndioContext(file));
  TRY(cctx=CCTX(self));
  // read first frame to update dimensions from first packet if necessary
  { int fin=0;
    do{
      av_free_packet(&packet);
      AVTRY(av_read_frame(self->fmt,&packet),"Failed to read frame.");
    } while(packet.stream_index!=self->istream);
    AVTRY(avcodec_decode_video2(cctx,self->raw,&fin,&packet),"Failed to decode frame.");
    av_free_packet(&packet);
    AVTRY(av_seek_frame(self->fmt,self->istream,0,AVSEEK_FLAG_BACKWARD/*flags*/),"Failed to seek to beginning.");
  }
  d=(int)self->nframes;
  w=cctx->width;
  h=cctx->height;
  TRY(pixfmt_to_nd_type(pixfmt_to_output_pixfmt(cctx->pix_fmt),&type,&c));
  { nd_t out=ndinit();
    size_t k,shape[]={w,h,d,c};
    k=pack(shape,countof(shape));
    ndcast(out,type);
    ndreshape(out,(unsigned)k,shape);
    return out;
  }
Error:
  av_free_packet(&packet);
  return NULL;
}

/// @cond DEFINES
#if 0
#define DEBUG_PRINT_PACKET_INFO \
    printf("Packet - pts:%5d dts:%5d (%5d) - flag: %1d - finished: %3d - Frame pts:%5d %5d\n",   \
        (int)packet.pts,(int)packet.dts,iframe,                                                  \
        packet.flags,finished,                                                                   \
        (int)self->raw->pts,(int)self->raw->best_effort_timestamp)
#else
#define DEBUG_PRINT_PACKET_INFO
#endif
/// @endcond

/** Fills \a p with zeros. */
static void zero(AVFrame *p)
{ int i,j;
  for(j=0;j<4;++j)
    if(p->data[j])
      for(i=0;i<p->height;++i)
        memset(p->data[j]+p->linesize[j]*i,0,p->linesize[j]);
}

/** Parse next packet from current video.
    Advances to the next frame.

    Caller is responsible for passing the correct, pre-allocated plane.

    \returns 1 on success, 0 otherwise.
 */
static int next(ndio_t file,nd_t plane,int64_t iframe)
{ ndio_ffmpeg_t self;
  AVPacket packet = {0};
  int yielded = 0;
  nd_t tmp=ndinit();
  ndreshapev(tmp,2,640,480);
  TRY(self=(ndio_ffmpeg_t)ndioContext(file));
  do
  { yielded=0;
    av_free_packet( &packet ); // no op when packet is null
    AVTRY(av_read_frame(self->fmt,&packet),"Failed to read frame.");   // !!NOTE: see docs on packet.convergence_duration for proper seeking
    if(packet.stream_index!=self->istream)
      continue;
    AVTRY(avcodec_decode_video2(CCTX(self),self->raw,&yielded,&packet),NULL);    
    // Handle odd cases and debug
    if(CCTX(self)->codec_id==CODEC_ID_RAWVIDEO)
    { if(!yielded) zero(self->raw); // Emit a blank frame. Something off about the stream.  Raw should always yield.           
      yielded=1;
    }
    DEBUG_PRINT_PACKET_INFO;
    if(!yielded && packet.size==0) // packet.size==0 usually means EOF
        break;
  } while(!yielded || self->raw->best_effort_timestamp<iframe);
  self->iframe=iframe;

  /*  === Copy out data, translating to desired pixel format ===
      Assume colors are last dimension.
      Assume plane points to start of image for first color.
      Assume at most four color planes.
      Assume each color plane has identical stride.
      Plane has full dimensionality of parent array; just offset.
  */
  { uint8_t *planes[4]={0};
    int lines[4]={0};
    const int lst = (int) ndstrides(plane)[1],
              cst = (int) ndstrides(plane)[ndndim(plane)-1];
    int i;
    for(i=0;i<countof(planes);++i)
    { lines[i]=lst;
      planes[i]=(uint8_t*)nddata(plane)+cst*i;
    }
    sws_scale(self->sws,              // sws context
              (const uint8_t*const*)self->raw->data, // src slice
              self->raw->linesize,    // src stride
              0, // src slice origin y
              CCTX(self)->height,     // src slice height
              planes,                 // dst
              lines);                 // dst line stride
  }
  av_free_packet(&packet); // For rawvideo, the packet.data is referenced by raw->data, so free here.
  ndfree(tmp);
  return 1;
Error:
  av_free_packet( &packet );
  return 0;
}

/** \returns current frame on success, otherwise -1 */
static int seek(ndio_t file, int64_t iframe)
{ ndio_ffmpeg_t self;
  int64_t duration,ts;
  TRY(self=(ndio_ffmpeg_t)ndioContext(file));
  duration = DURATION(self);
  ts = iframe; //av_rescale(duration,iframe,self->nframes);

  TRY(iframe>=0 && iframe<self->nframes);
  // AVSEEK_FLAG_BACKWARD determines the direction to go from the sought timestamp
  // to find a keyframe.
  AVTRY(avformat_seek_file( self->fmt,       //format context
                            self->istream,   //stream id
                            0,ts,ts,          //min,target,max timestamps
                            AVSEEK_FLAG_BACKWARD|AVSEEK_FLAG_FRAME),//flags
                            "Failed to seek.");
//(necessary?)  avcodec_flush_buffers(CCTX(self));
  return 1;
Error:
  return 0;
}

/** Returns the number of frames in \a file */
static int64_t nframes(const ndio_t file)
{ ndio_ffmpeg_t self;
  TRY(self=(ndio_ffmpeg_t)ndioContext(file));
  return self->nframes;
Error:
  return 0;
}

/**
  Reads the data in \a file into the array \a a.
  The caller must allocate \a a, make sure it has the correct shape,
  kind, and references a big enough destination buffer.

  Assumes:
    1. Output ordering is w,h,d,c
    2. Array container has the correct size and type
*/
static unsigned read_ffmpeg(ndio_t file, nd_t a)
{ int64_t i;
  void *o=nddata(a);
  TRY(seek(file,0));
  for(i=0;i<nframes(file);++i,ndoffset(a,2,1))
    TRY(next(file,a,i));
  ndref(a,o,ndkind(a));
  return 1;
Error:
  return 0;
}

/**
 * Query seekable dimensions.
 * Output ordering is w,h,d,c.
 * Only d is seekable.
 */
static unsigned canseek_ffmpeg(ndio_t file, size_t idim)
{return idim==2;}

/**
 * Seek
 */
static unsigned seek_ffmpeg(ndio_t file,nd_t a,size_t *pos)
{ ndio_ffmpeg_t self;
  size_t i;
  TRY(self=(ndio_ffmpeg_t)ndioContext(file));
  TRY(ndndim(a)>=2);
  i=(ndndim(a)>2)?pos[2]:0;
  if(i!=self->iframe+1)
    TRY(seek(file,i));
  TRY(next(file,a,i));
  return 1;
Error:
  return 0;
}

static void argmin_sz(size_t n, const size_t *v, size_t *argmin, size_t *min)
{ size_t i,am=0,m=v[0];
  for(i=1;i<n;++i) if(v[i]<m) m=v[am=i];
  if(argmin) *argmin=am;
  if(min)    *min=m;
}

/**
  Writes the data in \a to the file \a file.

  Appends d planes of a to the stream

  Note: May want special handling for 3d data.  Should the 3d be treated as x,y,color or x,y,depth?
 */
static unsigned write_ffmpeg(ndio_t file, nd_t a)
{ ndio_ffmpeg_t self;
  nd_t arg=a;
  int c,w,h,d,i,isok=1;
  const size_t *s;
  size_t planestride,colorstride;
  int linestride;
  int pixfmt;
  AVPacket p={0};
  int got_packet;
  nd_type_id_t oldtype=nd_id_unknown;
  nd_t tmp=0; ///< A temporary copy of a may be needed if a transpose is required for channel ordering
  AVCodecContext *cctx;

  TRY(self=(ndio_ffmpeg_t)ndioContext(file));
  cctx=CCTX(self);
  s=ndshape(a);
  
  { // maybe flip signed ints to unsigned
    static const nd_type_id_t tmap[] = {
      nd_id_unknown,nd_id_unknown,nd_id_unknown,nd_id_unknown,
      nd_u8        ,nd_u16       ,nd_u32       ,nd_u64,
      nd_id_unknown,nd_id_unknown};
    nd_type_id_t t = tmap[ndtype(a)];
    if(t>nd_id_unknown)
    { oldtype=ndtype(a);
      TRY(ndconvert_ip(a,t));
    }
  }
 
  switch(ndndim(a)) // Map dimensions to space, time and color
  { case 2:      c=1;         w=(int)s[0]; h=(int)s[1]; d=1;         break;// w,h
    case 3:      c=1;         w=(int)s[0]; h=(int)s[1]; d=(int)s[2]; break;// w,h,d
    case 4:
      // Try to guess which dimension is the color dimension (hint: it's the smallest one of size 1,2,3 or 4)
      { size_t cdim,nc;
        argmin_sz(ndndim(a),s,&cdim,&nc);
        if(nc>4)
          FAIL("Unsupported number of color components");
        TRY(ndcast(ndreshape(tmp=ndinit(),ndndim(a),ndshape(a)),ndtype(a)));
        if(nc==2 || cdim!=0)
        { if(nc==2) ndShapeSet(tmp,cdim,3); // need to pad to add the third color for RGB
          TRY(ndfill(ndref(tmp,malloc(ndnbytes(tmp)),nd_heap),0));
        }
        // Then, if necessary, transpose to put color on dim 0 and go from there.
        if(cdim!=0)
          TRY(ndshiftdim(tmp,a,-cdim));
        if(tmp)
        { a=tmp;            // !!! ALIAS ARRAY TO TMP
          s=ndshape(tmp);   // !!! This is by design, but it is confusing.
        }        
        c=(int)s[0]; w=(int)s[1]; h=(int)s[2]; d=(int)s[3];// c,w,h,d
      }
      break;
    default:
      FAIL("Unsupported number of dimensions.");
  }
  planestride=ndstrides(a)[ndndim(a)-1];
  linestride=(int)ndstrides(a)[ndndim(a)-2];
  colorstride=ndstrides(a)[0];
  TRY(PIX_FMT_NONE!=(pixfmt=to_pixfmt((int)colorstride,c)));
  TRY(maybe_init_codec_ctx(self,w,h,24,pixfmt));
  if(self->raw->pts==AV_NOPTS_VALUE)
    self->raw->pts=0;
  for(i=0;i<d;++i,++self->raw->pts)
  { AVFrame *in;
    av_init_packet(&p); // FIXME: for efficiency, probably want to preallocate packet
    { const uint8_t* plane=((uint8_t*)nddata(a))+planestride*i;
      const uint8_t* slice[4]={ plane+colorstride*0,
                                plane+colorstride*1,
                                plane+colorstride*2,
                                plane+colorstride*3};
      const int stride[4]={linestride,linestride,linestride,linestride};
      in=self->raw;
      sws_scale(self->sws,slice,stride,0,h,self->raw->data,self->raw->linesize);
    }
    TRY(push(file,&p,self->raw,&got_packet));
  }

  // maybe flip back to signed ints
  if(oldtype>nd_id_unknown)
    TRY(ndconvert_ip(arg,oldtype));
Finalize:
  ndfree(tmp);
  return isok;
Error:
  isok=0;
  goto Finalize;
}

//
//  === EXPORT ===
//

/// @cond DEFINES
#ifdef _MSC_VER
#define shared __declspec(dllexport)
#else
#define shared
#endif
/// @endcond

/** Expose the interface as an ndio plugin. */
shared const ndio_fmt_t* ndio_get_format_api(void)
{ static ndio_fmt_t api = {0};
  maybe_init();
  api.name   = name_ffmpeg;
  api.is_fmt = is_ffmpeg;
  api.open   = open_ffmpeg;
  api.close  = close_ffmpeg;
  api.shape  = shape_ffmpeg;
  api.read   = read_ffmpeg;
  api.write  = write_ffmpeg;
  api.canseek= canseek_ffmpeg;
  api.seek   = seek_ffmpeg;
  api.add_plugin=ndioAddPlugin; // useful for dumping files for debugging
  return &api;
}

