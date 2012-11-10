/*
Copyright (c) 2011, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "SkRecordingData.h"

SkRecordingData::SkRecordingData()
    :   n_save(0),
        n_savelayer(0),
        n_restore(0),
        n_translate(0),
        n_scale(0),
        n_rotate(0),
        n_skew(0),
        n_concat(0),
        n_setmatrix(0),
        n_cliprect(0),
        n_clippath(0),
        n_clipregion(0),
        n_drawpaint(0),
        n_drawpoints(0),
        n_drawrect(0),
        n_drawpath(0),
        n_drawbitmap(0),
        n_drawbitmaprect(0),
        n_drawbitmapmat(0),
        n_drawsprite(0),
        n_drawtext(0),
        n_drawpostext(0),
        n_drawpostexth(0),
        n_drawtextpath(0),
        n_drawpicture(0),
        n_drawshape(0),
        n_drawvertices(0),
        n_drawdata(0),
        n_compressedbitmaps(0)
{}

SkRecordingData::~SkRecordingData()
{}

//reset data
void SkRecordingData::reset()
{
    n_save = n_savelayer = n_restore = n_translate = n_scale = n_rotate = 0;
    n_skew = n_concat = n_setmatrix = n_cliprect = n_clippath = 0;
    n_clipregion = n_drawpaint = n_drawpoints = n_drawrect = 0;
    n_drawpath = n_drawbitmap = n_drawbitmaprect = n_drawbitmapmat = 0;
    n_drawsprite = n_drawtext = n_drawpostext = n_drawpostexth = 0;
    n_drawtextpath = n_drawpicture = n_drawshape = n_drawvertices = 0;
    n_drawdata = n_compressedbitmaps = 0;

}
