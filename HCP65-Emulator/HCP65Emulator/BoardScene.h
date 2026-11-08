#ifndef __BOARD_SCENE_H__
#define __BOARD_SCENE_H__
#include "RasterLib/Rasteriser.h"
#include "SupportLib/Image.h"
#include "WindowLib/CanvasDraw.h"
#include "WindowLib/ImageCanvasDraw.h"
#include "WindowLib/WindowTheme.h"
#include "HCP65Lib/HCP65.h"


#define BOARD_SCENE_WIDTH		640
#define BOARD_SCENE_HEIGHT		420		//With the combo boxes under it the dialog just fits over the emulator's window.
#define BOARD_SCENE_NUM_MODELS	5		//The Small Mainboard and the boards CHCP65Mainboard can make.


//The Small Mainboard's 3D model with the boards chosen for its sockets standing in them, turning, in perspective with the
//eye 30 degrees up from the mainboard's plane.  The same board in two sockets is one model drawn twice (SRasterInstance).
//
//The models are KiCad's VRML exports, which take seconds to read.  One is only needed when it is shown, when it's read,
//cleaned up and simplified to about what a pixel is here, and saved to Output/Models (CRasterMesh::Save), from where it
//loads at once from then on, until its export changes.  LoadNext does one at a time so that the window can paint between
//them.  They stay in memory, so make this once.
//
//Where a board goes comes from the KiCad boards: see gasBoardSceneSockets and gasBoardSceneModels.
class CBoardSceneDraw : public CCanvasDraw
{
CONSTRUCTABLE(CBoardSceneDraw);
DESTRUCTABLE(CBoardSceneDraw);
protected:
	Ptr<CImageCanvasDraw>	mpCelDraw;
	Ptr<CImage>				mpImage;		//What the rasteriser draws on.
	CRasterMesh				macModels[BOARD_SCENE_NUM_MODELS];
	bool					mabTried[BOARD_SCENE_NUM_MODELS];	//It has been loaded, or couldn't be.
	SRasterInstance			masInstances[HCP65_NUM_DEVICE_SLOTS + 1];
	size					muiNumInstances;
	CRasterCamera			mcCamera;
	CRasteriser				mcRasteriser;
	CRasterTarget			mcTarget;
	uint32					muiBackground;	//0xAARRGGBB.
	float32					mfYaw;
	bool					mbFitted;		//The camera has been fitted to the mainboard.
	bool					mbDirty;

public:
	void		Init(Ptr<CWindowTheme> pTheme);
	void		Class(void) override;
	void		Free(void) override;
	bool		Save(CObjectWriter* pcFile) override;
	bool		Load(CObjectReader* pcFile) override;

	bool		Draw(Ptr<CCanvas> pCanvas) override;
	SInt32Vec2	GetDesiredSize(void) override;

	bool		LoadNext(const char** aszTypes);
	void		SetBoards(const char** aszTypes);
	void		SetYaw(float32 fYaw);

	static bool	HasMainboardModel(void);

protected:
	bool		LoadModel(int iModel);
	void		AddInstance(int iModel, SInt32Vec3* psOffset, bool bStanding);
	void		FitCamera(void);
};


#endif // __BOARD_SCENE_H__
