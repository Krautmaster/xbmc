
#pragma once
/*
 *      Copyright (C) 2005-2009 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "DVDVideoCodec.h"
#include "DVDVideoCodecFFmpeg.h"
#include "libavcodec/vdpau.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <queue>
#include "threads/CriticalSection.h"
#include "pthread.h"
#include "semaphore.h"
#include "settings/VideoSettings.h"
#include "threads/Thread.h"
namespace Surface { class CSurface; }

#define NUM_RENDERBUF_PICS                 4 // ensure aligned with renderer buffer number
#define NUM_OUTPUT_PICS                    11 // max length of picture queue from migration to output surface (from video surface) through to the buffer being copied to back buffer, must be at least one more than NUM_RENDERBUF_PICS
//#define NUM_OUTPUT_SURFACES                9
#define NUM_OUTPUT_SURFACES                11 // number of allocated output surfaces, actual number used for non-pixmap should ideally match NUM_OUTPUT_PICS, and for pixmap should ideally be NUM_OUTPUT_PICS - NUM_RENDER_BUFPICS

//#define NUM_VIDEO_SURFACES_MPEG2           10  // (1 frame being decoded, 2 reference)
//#define NUM_VIDEO_SURFACES_H264            32 // (1 frame being decoded, up to 16 references)
//#define NUM_VIDEO_SURFACES_VC1             10  // (same as MPEG-2)
//#define NUM_OUTPUT_SURFACES_FOR_FULLHD     9
#define FULLHD_WIDTH                       1920
#define MAX_PIC_Q_LENGTH                   20 //for non-interop_yuv this controls the max length of the decoded pic to render completion Q

class CVDPAU
 : public CDVDVideoCodecFFmpeg::IHardwareDecoder, public CThread
{
public:

  struct pictureAge
  {
    int b_age;
    int ip_age[2];
  };

  struct Desc
  {
    const char *name;
    uint32_t id;
    uint32_t aux; /* optional extra parameter... */
  };

  void ShiftRenderSurfaces();

  CVDPAU();
  virtual ~CVDPAU();
  
  virtual bool Open      (AVCodecContext* avctx, const enum PixelFormat, unsigned int surfaces = 0);
  virtual int  Decode    (AVCodecContext* avctx, AVFrame* frame) {return Decode(avctx, frame, false, false);};
  virtual int  Decode    (AVCodecContext* avctx, AVFrame* frame, bool bSoftDrain = false, bool bHardDrain = false);
  virtual bool GetPicture(AVCodecContext* avctx, AVFrame* frame, DVDVideoPicture* picture);
  virtual void Reset();
  virtual void Close();
  virtual bool AllowFrameDropping();
  virtual void SetDropState(bool bDrop);
  virtual bool FreeResources(bool test = false);
  virtual bool QueueIsFull(bool wait = false);

  virtual int  Check(AVCodecContext* avctx);
  virtual const std::string Name() { return "vdpau"; }

  void SetWidthHeight(int width, int height);
  bool MakePixmap(int index, int width, int height);
  bool MakePixmapGL(int index);

  void ReleasePixmap(int flipBufferIdx);
  void BindPixmap(int flipBufferIdx);
  int PreBindAllPixmaps();
  bool IsBufferValid(int flipBufferIdx);
  void GLFinish();

  PFNGLXBINDTEXIMAGEEXTPROC    glXBindTexImageEXT;
  PFNGLXRELEASETEXIMAGEEXTPROC glXReleaseTexImageEXT;

#ifdef GL_NV_vdpau_interop
  PFNGLVDPAUINITNVPROC glVDPAUInitNV;
  PFNGLVDPAUFININVPROC glVDPAUFiniNV;
  PFNGLVDPAUREGISTEROUTPUTSURFACENVPROC glVDPAURegisterOutputSurfaceNV;
  PFNGLVDPAUREGISTERVIDEOSURFACENVPROC glVDPAURegisterVideoSurfaceNV;
  PFNGLVDPAUISSURFACENVPROC glVDPAUIsSurfaceNV;
  PFNGLVDPAUUNREGISTERSURFACENVPROC glVDPAUUnregisterSurfaceNV;
  PFNGLVDPAUSURFACEACCESSNVPROC glVDPAUSurfaceAccessNV;
  PFNGLVDPAUMAPSURFACESNVPROC glVDPAUMapSurfacesNV;
  PFNGLVDPAUUNMAPSURFACESNVPROC glVDPAUUnmapSurfacesNV;
  PFNGLVDPAUGETSURFACEIVNVPROC glVDPAUGetSurfaceivNV;
#endif

  GLuint GLGetSurfaceTexture(int plane, int field, int flipBufferIdx);
  int SetTexture(int plane, int field, int flipBufferIdx);
  GLuint GetTexture();
  GLuint m_glTexture;
//  virtual long Release();
  ThreadIdentifier m_renderThread;

  static void             FFReleaseBuffer(AVCodecContext *avctx, AVFrame *pic);
  static void             FFDrawSlice(struct AVCodecContext *s,
                               const AVFrame *src, int offset[4],
                               int y, int type, int height);
  static int              FFGetBuffer(AVCodecContext *avctx, AVFrame *pic);

  static void             VDPPreemptionCallbackFunction(VdpDevice device, void* context);

  void Present(int flipBufferIdx);
  bool ConfigVDPAU(AVCodecContext *avctx, int ref_frames);
  void SpewHardwareAvailable();
  void InitCSCMatrix(int Height);
  bool CheckStatus(VdpStatus vdp_st, int line);

  bool CheckRecover(bool force = false);
  void CheckFeatures();
  void SetPostProcFeatures(bool postProcEnabled = true);
  void SetColor();
  void SetNoiseReduction();
  void SetSharpness();
  void SetDeintSkipChroma();
  void SetDeinterlacing();
  void DisableHQScaling();
  void PostProcOff();
  EINTERLACEMETHOD GetDeinterlacingMethod(bool log = false);
  void SetHWUpscaling();
  bool DiscardPresentPicture();

  pictureAge picAge;
  volatile bool  recover;
  bool       clearedDown;
  vdpau_render_state *past[2], *current, *future[2];
  int        tmpDeintMode, tmpDeint;
  int        tmpUpScale;
  bool       tmpPostProc;
  float      tmpNoiseReduction, tmpSharpness;
  float      tmpBrightness, tmpContrast;
  int        OutWidth, OutHeight;
  int        upScale;
  bool m_preBindPixmapsDone;

  static inline void ClearUsedForRender(vdpau_render_state **st)
  {
    if (*st) {
      (*st)->state &= ~FF_VDPAU_STATE_USED_FOR_RENDER;
      *st = NULL;
    }
  }

  VdpProcamp    m_Procamp;
  VdpCSCMatrix  m_CSCMatrix;
  VdpDevice     HasDevice() { return vdp_device != VDP_INVALID_HANDLE; };
  VdpChromaType vdp_chroma_type;
  bool GenerateStudioCSCMatrix(VdpColorStandard colorStandard, VdpCSCMatrix &studioCSCMatrix);


  //  protected:
  void  InitVDPAUProcs();
  void  FiniVDPAUProcs();
  void  FiniVDPAUOutput();
  bool  ConfigOutputMethod(AVCodecContext *avctx, AVFrame *pFrame);
  bool  FiniOutputMethod();

  VdpDevice                            vdp_device;
  VdpGetProcAddress *                  vdp_get_proc_address;
  VdpDeviceDestroy *                   vdp_device_destroy;

  VdpVideoSurfaceCreate *              vdp_video_surface_create;
  VdpVideoSurfaceDestroy *             vdp_video_surface_destroy;
  VdpVideoSurfacePutBitsYCbCr *        vdp_video_surface_put_bits_y_cb_cr;
  VdpVideoSurfaceGetBitsYCbCr *        vdp_video_surface_get_bits_y_cb_cr;

  VdpOutputSurfacePutBitsYCbCr *        vdp_output_surface_put_bits_y_cb_cr;
  VdpOutputSurfacePutBitsNative *       vdp_output_surface_put_bits_native;
  VdpOutputSurfaceCreate *              vdp_output_surface_create;
  VdpOutputSurfaceDestroy *             vdp_output_surface_destroy;
  VdpOutputSurfaceGetBitsNative *       vdp_output_surface_get_bits_native;
  VdpOutputSurfaceRenderOutputSurface * vdp_output_surface_render_output_surface;
  VdpOutputSurfacePutBitsIndexed *      vdp_output_surface_put_bits_indexed;

  VdpVideoMixerCreate *                vdp_video_mixer_create;
  VdpVideoMixerSetFeatureEnables *     vdp_video_mixer_set_feature_enables;
  VdpVideoMixerQueryParameterSupport * vdp_video_mixer_query_parameter_support;
  VdpVideoMixerQueryFeatureSupport *   vdp_video_mixer_query_feature_support;
  VdpVideoMixerDestroy *               vdp_video_mixer_destroy;
  VdpVideoMixerRender *                vdp_video_mixer_render;
  VdpVideoMixerSetAttributeValues *    vdp_video_mixer_set_attribute_values;

  VdpGenerateCSCMatrix *               vdp_generate_csc_matrix;

  VdpPresentationQueueTargetDestroy *         vdp_presentation_queue_target_destroy;
  VdpPresentationQueueCreate *                vdp_presentation_queue_create;
  VdpPresentationQueueDestroy *               vdp_presentation_queue_destroy;
  VdpPresentationQueueDisplay *               vdp_presentation_queue_display;
  VdpPresentationQueueBlockUntilSurfaceIdle * vdp_presentation_queue_block_until_surface_idle;
  VdpPresentationQueueTargetCreateX11 *       vdp_presentation_queue_target_create_x11;
  VdpPresentationQueueQuerySurfaceStatus *    vdp_presentation_queue_query_surface_status;
  VdpPresentationQueueGetTime *               vdp_presentation_queue_get_time;

  VdpGetErrorString *                         vdp_get_error_string;

  VdpDecoderCreate *            vdp_decoder_create;
  VdpDecoderDestroy *           vdp_decoder_destroy;
  VdpDecoderRender *            vdp_decoder_render;
  VdpDecoderQueryCapabilities * vdp_decoder_query_caps;

  VdpPreemptionCallbackRegister * vdp_preemption_callback_register;

  VdpOutputSurface  outputSurfaces[NUM_OUTPUT_SURFACES];
  VdpOutputSurface  presentSurface;

  VdpDecoder    decoder;
  VdpVideoMixer videoMixer;
  VdpRect       outRectVid;

  static void*    dl_handle;
  VdpStatus (*dl_vdp_device_create_x11)(Display* display, int screen, VdpDevice* device, VdpGetProcAddress **get_proc_address);
  VdpStatus (*dl_vdp_get_proc_address)(VdpDevice device, VdpFuncId function_id, void** function_pointer);
  VdpStatus (*dl_vdp_preemption_callback_register)(VdpDevice device, VdpPreemptionCallback callback, void* context);


  int      totalAvailableOutputSurfaces;
  uint32_t vid_width, vid_height;
  uint32_t max_references;
  Display* m_Display;
  bool     vdpauConfigured;
  bool     m_bPixmapBound;


  VdpVideoMixerPictureStructure m_mixerfield;
  int                           m_mixerstep;

  bool Supports(VdpVideoMixerFeature feature);
  bool Supports(EINTERLACEMETHOD method);

  VdpVideoMixerFeature m_features[10];
  int                  m_feature_count;

  static bool IsVDPAUFormat(PixelFormat fmt);
  static void ReadFormatOf( PixelFormat fmt
                          , VdpDecoderProfile &decoder_profile
                          , VdpChromaType     &chroma_type);

  std::vector<vdpau_render_state*> m_videoSurfaces;

protected:
  virtual void OnStartup();
  virtual void OnExit();
  virtual void Process();
  void FlushMixer();
//  int NextBuffer();

  struct MixerMessage
  {
    DVDVideoPicture DVDPic;
    vdpau_render_state * render;
    VdpRect outRectVid;
  };

  struct OutputPicture
  {
    DVDVideoPicture DVDPic;
    VdpOutputSurface outputSurface;
    vdpau_render_state * render;
    GLuint texture[4];
    GLXPixmap  glPixmap;
    Pixmap  pixmap;
    bool bound;
    bool reported;
    VdpPresentationQueueTarget vdp_flip_target;
    VdpPresentationQueue vdp_flip_queue;
#ifdef GL_NV_vdpau_interop
    GLvdpauSurfaceNV glVdpauSurface;
#endif
  };

  struct GLVideoSurface
  {
    GLuint texture[4];
#ifdef GL_NV_vdpau_interop
    GLvdpauSurfaceNV glVdpauSurface;
#endif
  };

  std::queue<MixerMessage> m_mixerMessages;
  std::deque<MixerMessage> m_mixerInput;
  std::map<VdpVideoSurface, GLVideoSurface> m_videoSurfaceMap;
  OutputPicture m_allOutPic[NUM_OUTPUT_PICS];
  std::deque<OutputPicture*> m_freeOutPic;
  std::deque<OutputPicture*> m_usedOutPic;
  OutputPicture *m_presentPicture;
  OutputPicture *m_flipBuffer[NUM_RENDERBUF_PICS];
  unsigned int m_mixerCmd;
  CCriticalSection m_mixerSec, m_outPicSec, m_videoSurfaceSec, m_flipSec;
  CEvent m_picSignal;
  CEvent m_msgSignal;
  CEvent m_queueSignal;
  bool m_bVdpauDeinterlacing;
  bool m_binterlacedFrame;
  int m_outPicsNum;
  int m_dropCount;
  volatile bool glInteropFinish;
  bool m_dropState;
  bool m_bPostProc;
  uint32_t *m_threeBlackLines;

  enum VDPAUOutputMethod
  {
    OUTPUT_NONE,
    OUTPUT_PIXMAP,
    OUTPUT_GL_INTEROP_RGB,
    OUTPUT_GL_INTEROP_YUV
  };
  VDPAUOutputMethod m_vdpauOutputMethod;
  VDPAUOutputMethod m_GlInteropStatus;

#ifdef GL_NV_vdpau_interop
  void GLInitInterop();
  void GLFiniInterop();
  bool GLMapSurface(OutputPicture *outPic);
  bool GLUnmapSurface(OutputPicture *outPic);
  bool GLRegisterVideoSurfaces(OutputPicture *outPic);
  bool GLRegisterOutputSurfaces();
#endif

#define MIXER_CMD_FLUSH	0x01
#define MIXER_CMD_HURRY	0x02
#define MIXER_CMD_DRAIN	0x04
};
