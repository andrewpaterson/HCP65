#ifndef __BOARD_SCENE_H__
#define __BOARD_SCENE_H__
#include "RasterLib/Rasteriser.h"
#include "SupportLib/Image.h"
#include "ThreadLib/WorkerThreadJobs.h"
#include "WindowLib/CanvasDraw.h"
#include "WindowLib/ImageCanvasDraw.h"
#include "WindowLib/WindowTheme.h"
#include "HCP65Lib/HCP65.h"


#define BOARD_SCENE_WIDTH		640
#define BOARD_SCENE_HEIGHT		782		//With the combo boxes under it the dialog fits over the emulator's window.
#define BOARD_SCENE_WORKERS		8		//One a core here.  More only added hyperthreads, for 4ms a frame.
#define BOARD_SCENE_NUM_MODELS	6		//The Small Mainboard, the Processor Board and the boards CHCP65Mainboard can make.
#define BOARD_SCENE_NUM_INSTANCES	(HCP65_NUM_DEVICE_SLOTS + 2)	//The mainboard, the Processor Board and a board for each socket.


class CMesh;


//The Small Mainboard's 3D model with the Processor Board in its socket and the boards chosen for its other sockets
//standing in them, turning, in perspective with the eye 30 degrees up from the mainboard's plane.  The same board in two
//sockets is one model drawn twice (SRasterInstance).
//
//The models are KiCad's VRML exports, which take seconds to read.  One is only needed when it is shown, when it's read,
//cleaned up and simplified to about what a pixel is here, and its mesh saved to Output/Models as a scene file (.DRG), from
//where it loads in under a second from then on, until its export is newer.  LoadNext does one at a time so that the window
//can paint between them.  They stay in memory, so make this once.
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
	SRasterInstance			masInstances[BOARD_SCENE_NUM_INSTANCES];
	size					muiNumInstances;
	CRasterCamera			mcCamera;
	CRasteriser				mcRasteriser;
	CWorkerThreadJobs		mcJobs;
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
	bool		LoadSavedModel(int iModel, const char* szSaved);
	bool		LoadSourceModel(int iModel, const char* szSaved);
	void		BuildModel(int iModel, CMesh* pcMesh);
	void		AddInstance(int iModel, SInt32Vec3* psOffset, bool bStanding);
	void		AddStandingBoard(int iModel, float64 fFootprintY);
	void		FitCamera(void);
};


#endif // __BOARD_SCENE_H__
