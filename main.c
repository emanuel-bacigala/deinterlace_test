#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bcm_host.h"
#include "ilclient.h"

#define FPS	2

#define OMX_INIT_STRUCTURE(a) \
memset(&(a), 0, sizeof(a)); \
(a).nSize = sizeof(a); \
(a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
(a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
(a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
(a).nVersion.s.nStep = OMX_VERSION_STEP


OMX_TICKS ToOMXTime(int64_t pts)
{
    OMX_TICKS ticks;
    ticks.nLowPart = pts;
    ticks.nHighPart = pts >> 32;

    return ticks;
}


int video_deinterlace_test(void)
{
    ILCLIENT_T *client;
    COMPONENT_T *image_fx = NULL, *video_scheduler = NULL, *video_render = NULL, *clock = NULL;
    COMPONENT_T *list[4+1];
    TUNNEL_T tunnel[3+1];

    memset(list, 0, sizeof(list));
    memset(tunnel, 0, sizeof(tunnel));

    if ((client = ilclient_init()) == NULL)
    {
      return 1;
    }

    if (OMX_Init() != OMX_ErrorNone)
    {
        ilclient_destroy(client);
        return 1;
    }

  // create image_fx
    if (ilclient_create_component(client, &image_fx, "image_fx", ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: ilclient_create_component(image_fx) failed!\n");
        return 1;
    }

    list[0] = image_fx;
/*
   // disable proprietary tunneling
    OMX_PARAM_BRCMDISABLEPROPRIETARYTUNNELSTYPE propTunnel;
    OMX_INIT_STRUCTURE(propTunnel);
    propTunnel.nPortIndex = 191;
    propTunnel.bUseBuffers = OMX_TRUE;

    if (OMX_SetConfig (ILC_GET_HANDLE(image_fx), OMX_IndexParamBrcmDisableProprietaryTunnels, &propTunnel) != OMX_ErrorNone)
    {
        fprintf (stderr, "Error: OMX_SetConfig(image_fx,OMX_IndexParamBrcmDisableProprietaryTunnels) failed!\n");
        return 1;
    }
*/
/*
  // extra buffers ?
   OMX_PARAM_U32TYPE extra_buffers;
   OMX_INIT_STRUCTURE(extra_buffers);
   extra_buffers.nU32 = -2;

   if (OMX_SetParameter(ILC_GET_HANDLE(image_fx), OMX_IndexParamBrcmExtraBuffers, &extra_buffers) != OMX_ErrorNone)
   {
       fprintf(stderr, "%s() - Error: OMX_SetParameter(image_fx, OMX_IndexParamBrcmExtraBuffers) failed!\n", __FUNCTION__);
       return 1;
   }
*/

  // create video_render
    if (ilclient_create_component(client, &video_render, "video_render", ILCLIENT_DISABLE_ALL_PORTS) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: ilclient_create_component(video_render) failed!\n");
        return 1;
    }

    list[1] = video_render;

  // create clock
    if (ilclient_create_component(client, &clock, "clock", ILCLIENT_DISABLE_ALL_PORTS) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: ilclient_create_component(clock) failed!\n");
        return 1;
    }

    list[2] = clock;

    OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;
    OMX_INIT_STRUCTURE(cstate);
    cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
    cstate.nWaitMask = 1;
    if (OMX_SetParameter(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_SetParameter(clock, OMX_IndexConfigTimeClockState) failed!\n");
        return 1;
    }

  // set clock scale
    OMX_TIME_CONFIG_SCALETYPE cscale;
    OMX_INIT_STRUCTURE(cscale);
    cscale.xScale = 0x00010000;  // 1x
    //cscale.xScale = 0x00080000;  // 8x
    //cscale.xScale = 0x00001000;  // 1/16x
    //cscale.xScale = 0x00000AAA;  // 1/24x
    //cscale.xScale = 0x00000A3D;  // 1/25x
    //cscale.xScale = 0x00000888;  // 1/30x

    if (OMX_SetParameter(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeScale, &cscale) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_SetParameter(clock,OMX_IndexConfigTimeScale) failed!\n");
        return 1;
    }

  // create video_scheduler
    if (ilclient_create_component(client, &video_scheduler, "video_scheduler", ILCLIENT_DISABLE_ALL_PORTS) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: ilclient_create_component(video_scheduler) failed!\n");
        return 1;
    }

    list[3] = video_scheduler;

    set_tunnel(tunnel, image_fx, 191, video_scheduler, 10);
    set_tunnel(tunnel+1, video_scheduler, 11, video_render, 90);
    set_tunnel(tunnel+2, clock, 80, video_scheduler, 12);

  // setup clock tunnel first
    if (ilclient_setup_tunnel(tunnel+2, 0, 0) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: ilclient_setup_tunnel(+2) failed!\n");
        return 1;
    }

    ilclient_change_component_state(clock, OMX_StateExecuting);
    ilclient_change_component_state(image_fx, OMX_StateIdle);

  // setup render
    OMX_CONFIG_DISPLAYREGIONTYPE configDisplay;
    OMX_INIT_STRUCTURE(configDisplay);
    configDisplay.nPortIndex = 90;

    configDisplay.set = (OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_TRANSFORM| OMX_DISPLAY_SET_ALPHA|
                                             OMX_DISPLAY_SET_LAYER    | OMX_DISPLAY_SET_NUM  /*|
                                             OMX_DISPLAY_SET_NOASPECT */);
    configDisplay.alpha = 255;
    configDisplay.num = 0;
    configDisplay.layer = (1<<15)-1;
    configDisplay.transform = 0;
    configDisplay.noaspect   = OMX_TRUE;

    if (OMX_SetConfig(ILC_GET_HANDLE(video_render), OMX_IndexConfigDisplayRegion, &configDisplay) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_SetConfig(video_render, OMX_IndexConfigDisplayRegion) failed!\n");
        return 1;
    }

  // need to setup image_fx input port
    OMX_PARAM_PORTDEFINITIONTYPE portdef;
    OMX_INIT_STRUCTURE(portdef);
    portdef.nPortIndex = 190;

    if (OMX_GetParameter(ILC_GET_HANDLE(image_fx), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_GetParameter(image_fx, OMX_IndexParamPortDefinition) failed!\n");
        return 1;
    }

    portdef.nBufferCountActual = 3;  // set input buffer count (max. 153 original 1)

    portdef.format.image.nFrameWidth = 720;
    portdef.format.image.nFrameHeight = 576;
    portdef.format.image.nStride = 736;
    portdef.format.image.nSliceHeight = 576;
    portdef.nBufferSize = portdef.format.image.nStride * portdef.format.image.nSliceHeight * 3 / 2;
    portdef.format.image.eCompressionFormat = OMX_VIDEO_CodingUnused; // OMX_VIDEO_CodingUnused OMX_IMAGE_CodingUnused
    portdef.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

    if (OMX_SetParameter(ILC_GET_HANDLE(image_fx), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_SetParameter(image_fx, OMX_IndexParamPortDefinition) failed!\n");
        return 1;
    }

    if (OMX_GetParameter(ILC_GET_HANDLE(image_fx), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_GetParameter(image_fx, OMX_IndexParamPortDefinition) failed!\n");
        return 1;
    }

    fprintf(stderr, "image_fx input buffer count=%d\n", portdef.nBufferCountActual);

    if (ilclient_enable_port_buffers(image_fx, 190, NULL, NULL, NULL) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: ilclient_enable_port_buffers(image_fx, 190) failed!\n");
        return 1;
    }
/*
  // without parameters
    OMX_CONFIG_IMAGEFILTERTYPE image_format;
    OMX_INIT_STRUCTURE(image_format);
    image_format.nPortIndex = 191;
    image_format.eImageFilter = OMX_ImageFilterNone; // OMX_ImageFilterDeInterlaceAdvanced
                                                                        // OMX_ImageFilterDeInterlaceLineDouble
                                                                        // OMX_ImageFilterDeInterlaceFast
                                                                        // OMX_ImageFilterNone
                                                                        // OMX_ImageFilterNegative
                                                                        // OMX_ImageFilterPastel
                                                                        // OMX_ImageFilterCartoon
                                                                        // OMX_ImageFilterEmboss

    if (OMX_SetConfig(ILC_GET_HANDLE(image_fx), OMX_IndexConfigCommonImageFilter, &image_format) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_SetConfig(image_fx, OMX_IndexConfigCommonImageFilter) failed!\n");
        return 1;
    }
*/

  // with parameters
    OMX_CONFIG_IMAGEFILTERPARAMSTYPE image_format;
    OMX_INIT_STRUCTURE(image_format);
    image_format.nPortIndex = 191;
    image_format.nNumParams = 4;
    image_format.nParams[0] = 3;
    image_format.nParams[1] = 0;
    image_format.nParams[2] = 0;
    image_format.nParams[3] = 1;
    //image_format.eImageFilter = OMX_ImageFilterNegative;
    //image_format.eImageFilter = OMX_ImageFilterPosterise;
    //image_format.eImageFilter = OMX_ImageFilterCartoon;
    //image_format.eImageFilter = OMX_ImageFilterFilm;
    //image_format.eImageFilter = OMX_ImageFilterBlur;
    //image_format.eImageFilter = OMX_ImageFilterPastel;
    //image_format.eImageFilter = OMX_ImageFilterOilPaint;
    //image_format.eImageFilter = OMX_ImageFilterSketch;
    //image_format.eImageFilter = OMX_ImageFilterDeInterlaceLineDouble;
    image_format.eImageFilter = OMX_ImageFilterDeInterlaceAdvanced;
    //image_format.eImageFilter = OMX_ImageFilterDeInterlaceFast;
    //image_format.eImageFilter = OMX_ImageFilterNone;


    if (OMX_SetConfig(ILC_GET_HANDLE(image_fx), OMX_IndexConfigCommonImageFilterParameters, &image_format) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_SetConfig(image_fx, OMX_IndexConfigCommonImageFilterParameters) failed!\n");
        return 1;
    }

    if (ilclient_setup_tunnel(tunnel, 0, 0) != 0)
    {
        fprintf(stderr, "Error: ilclient_setup_tunnel(+0) failed!\n");
        return 1;
    }

    ilclient_change_component_state(video_scheduler, OMX_StateExecuting);

  // now setup tunnel to video_render
    if (ilclient_setup_tunnel(tunnel+1, 0, 1000) != 0)
    {
        fprintf(stderr, "Error: ilclient_setup_tunnel(+1) failed!\n");
        return 1;
    }

    ilclient_change_component_state(video_render, OMX_StateExecuting);
    ilclient_change_component_state(image_fx, OMX_StateExecuting);

    OMX_BUFFERHEADERTYPE *buf;
    int first_packet = 1;
    int dts = 0; // simulation of time stamps (incremented after each frame)

    while ((buf = ilclient_get_input_buffer(image_fx, 190, 1)) != NULL)
    {
        unsigned char *imagePartPtr = buf->pBuffer;
        int width = portdef.format.image.nFrameWidth;
        int height = portdef.format.image.nFrameHeight;
        int linesize = portdef.format.image.nStride;
        int data_len;
        int row;
        int sizeToRead;

      // read Y component
        sizeToRead = width;
        row = 0;
        while ((data_len = read(0, imagePartPtr, sizeToRead)) > 0)
        {
            imagePartPtr += data_len;

            if (data_len < sizeToRead)
            {
                sizeToRead -= data_len;
                continue;
            }
            else
            {
                imagePartPtr += (linesize - width);
                sizeToRead = width;

                if (++row == height)  // have complete Y ?
                   break;
            }
        }

      // read UV component
        sizeToRead = width/2;
        row = 0;
        while ((data_len = read(0, imagePartPtr, sizeToRead)) > 0)
        {
            imagePartPtr += data_len;

            if (data_len < sizeToRead)
            {
                sizeToRead -= data_len;
                continue;
            }
            else
            {
                sizeToRead -= data_len;

                imagePartPtr += (linesize/2 - width/2);
                sizeToRead = width/2;

                if(++row == height)  // have complete UV ?
                   break;
            }
        }

        if (!data_len)
            break;

        buf->nFilledLen = buf->nAllocLen;
        buf->nOffset = 0;
        if (image_format.eImageFilter == OMX_ImageFilterDeInterlaceAdvanced ||
            image_format.eImageFilter == OMX_ImageFilterDeInterlaceFast ||
            image_format.eImageFilter == OMX_ImageFilterDeInterlaceLineDouble)
            buf->nFlags = OMX_BUFFERFLAG_INTERLACED | OMX_BUFFERFLAG_TOP_FIELD_FIRST;
        else
            buf->nFlags = 0;

        if (first_packet)
        {
             buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
             first_packet = 0;
        }
        else
        {
          //full-speed
            //buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;

         //use timeStamps
           buf->nTimeStamp = ToOMXTime((uint64_t)dts);
           dts += 1000000/FPS;  // intra frame delay in [us]
        }

        if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(image_fx), buf) != OMX_ErrorNone)
        {
            fprintf(stderr, "Error: OMX_EmptyThisBuffer(image_fx) failed!\n");
            break;
        }
    }

    fprintf(stderr, "out of processing loop...\n");

    buf->nFilledLen = 0;
    buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

    if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(image_fx), buf) != OMX_ErrorNone)
    {
        fprintf(stderr, "Error: OMX_EmptyThisBuffer(image_fx, OMX_BUFFERFLAG_EOS) failed!\n");
    }

    ilclient_wait_for_event(video_render, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0, ILCLIENT_BUFFER_FLAG_EOS, -1);

    ilclient_flush_tunnels(tunnel, 0);
    ilclient_disable_tunnel(tunnel);
    ilclient_disable_tunnel(tunnel+1);
    ilclient_disable_tunnel(tunnel+2);

    ilclient_disable_port_buffers(image_fx, 190, NULL, NULL, NULL);

    ilclient_teardown_tunnels(tunnel);

    ilclient_state_transition(list, OMX_StateIdle);
    ilclient_state_transition(list, OMX_StateLoaded);

    ilclient_cleanup_components(list);

    OMX_Deinit();
    ilclient_destroy(client);

    fprintf(stderr, "exit ok.\n");

    return 0;
}

int main (int argc, char **argv)
{
    bcm_host_init();

    return video_deinterlace_test();
}
