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

#ifndef SkRecordingData_DEFINED
#define SkRecordingData_DEFINED

class SkRecordingData{

    public:

        SkRecordingData();
        virtual ~SkRecordingData();

        virtual void reset();

        //increment data
        virtual void addSave()                  {   ++n_save;   }
        virtual void addSaveLayer()             {   ++n_savelayer;  }
        virtual void addRestore()               {   ++n_restore;    }
        virtual void addTranslate()             {   ++n_translate;  }
        virtual void addScale()                 {   ++n_scale;  }
        virtual void addRotate()                {   ++n_rotate; }
        virtual void addSkew()                  {   ++n_skew;   }
        virtual void addConcat()                {   ++n_concat; }
        virtual void addSetMatrix()             {   ++n_setmatrix;  }
        virtual void addClipRect()              {   ++n_cliprect;   }
        virtual void addClipPath()              {   ++n_clippath;   }
        virtual void addClipRegion()            {   ++n_clipregion; }
        virtual void addDrawPaint()             {   ++n_drawpaint;  }
        virtual void addDrawPoints()            {   ++n_drawpoints; }
        virtual void addDrawRect()              {   ++n_drawrect;   }
        virtual void addDrawPath()              {   ++n_drawpath;   }
        virtual void addDrawBitmap()            {   ++n_drawbitmap; }
        virtual void addDrawBitmapRect()        {   ++n_drawbitmaprect; }
        virtual void addDrawBitmapMat()         {   ++n_drawbitmapmat;  }
        virtual void addDrawSprite()            {   ++n_drawsprite; }
        virtual void addDrawText()              {   ++n_drawtext;   }
        virtual void addDrawPosText()           {   ++n_drawpostext;    }
        virtual void addDrawPosTextH()          {   ++n_drawpostexth;   }
        virtual void addDrawTextPath()          {   ++n_drawtextpath;   }
        virtual void addDrawPicture()           {   ++n_drawpicture;    }
        virtual void addDrawShape()             {   ++n_drawshape;  }
        virtual void addDrawVertices()          {   ++n_drawvertices;   }
        virtual void addDrawData()              {   ++n_drawdata;   }
        virtual void addCompressedBitmap()      {   ++n_compressedbitmaps;  }

        virtual bool canUseGpuRendering() const      {   return false;   }

    protected:
        //keep track of what kind of commands are being recorded
        //This is needed to determine if ogl rendering can be used to accelerate
        int n_save;
        int n_savelayer;
        int n_restore;
        int n_translate;
        int n_scale;
        int n_rotate;
        int n_skew;
        int n_concat;
        int n_setmatrix;
        int n_cliprect;
        int n_clippath;
        int n_clipregion;
        int n_drawpaint;
        int n_drawpoints;
        int n_drawrect;
        int n_drawpath;
        int n_drawbitmap;
        int n_drawbitmaprect;
        int n_drawbitmapmat;
        int n_drawsprite;
        int n_drawtext;
        int n_drawpostext;
        int n_drawpostexth;
        int n_drawtextpath;
        int n_drawpicture;
        int n_drawshape;
        int n_drawvertices;
        int n_drawdata;
        int n_compressedbitmaps;
};

#endif
