#ifndef __HCP65_SCREEN_DRAW_H__
#define __HCP65_SCREEN_DRAW_H__
#include "SupportLib/Image.h"
#include "WindowLib/CanvasDraw.h"
#include "WindowLib/ImageCanvasDraw.h"


class CHCP65VideoBoard;


//The HCP65's screen: the Video Board's last whole frame, a pixel to a pixel in the middle of the canvas, on the
//background.  Just the background while there is no Video Board or it has no picture.  Update, on the window's thread,
//takes a new frame when the board has drawn one (the board draws on the HCP65's thread and hands frames over under a
//lock).
class CHCP65ScreenDraw : public CCanvasDraw
{
CONSTRUCTABLE(CHCP65ScreenDraw);
DESTRUCTABLE(CHCP65ScreenDraw);
protected:
	Ptr<CImageCanvasDraw>	mpImageDraw;
	Ptr<CImage>				mpImage;		//NULL until there is a frame.
	ARGB32					msBackground;
	CHCP65VideoBoard*		mpcVideo;
	uint64					muiFrames;		//The board's frame count when its frame was last taken.
	bool					mbSignal;

public:
	void	Init(ARGB32 sBackground);
	void	Class(void) override;
	void	Free(void) override;

	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	Draw(Ptr<CCanvas> pCanvas) override;

	void	SetVideo(CHCP65VideoBoard* pcVideo);
	bool	Update(void);
};


#endif // __HCP65_SCREEN_DRAW_H__
