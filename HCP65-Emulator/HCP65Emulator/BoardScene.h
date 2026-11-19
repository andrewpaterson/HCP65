#ifndef __BOARD_SCENE_H__
#define __BOARD_SCENE_H__
#include "StandardLib/Array.h"
#include "RasterLib/Rasteriser.h"
#include "SupportLib/Image.h"
#include "SupportLib/MeshEditor.h"
#include "SupportLib/Scene.h"
#include "RenderLib/RenderScene.h"
#include "RasterRenderLib/RasterFrame.h"
#include "RasterRenderLib/RasterRenderFactory.h"
#include "ThreadLib/WorkerThreadJobs.h"
#include "WindowLib/CanvasDraw.h"
#include "WindowLib/ImageCanvasDraw.h"
#include "WindowLib/WindowTheme.h"
#include "HCP65Lib/HCP65.h"


#define BOARD_SCENE_WIDTH		640
#define BOARD_SCENE_HEIGHT		782		//With the combo boxes under it the dialog fits over the emulator's window.
#define BOARD_SCENE_WORKERS		8		//One a core here.  More only added hyperthreads, for 4ms a frame.
#define BOARD_SCENE_LOADERS		4		//The threads that read models: as many are read at once.
#define BOARD_SCENE_NUM_MODELS	10		//The Small Mainboard, the Processor Board and the boards CHCP65Mainboard can make.
#define BOARD_SCENE_NUM_SOCKETS	7		//The Processor Board's and the six the Small Mainboard has for devices.


//Where a model's load is.  The scene's thread and a loader's both read and write it (atomically).
enum EBoardSceneLoad
{
	BSL_NotTried,
	BSL_Loading,	//A loader's thread has it, or soon will.
	BSL_Loaded,		//Its editor is there for the scene to take.
	BSL_Taken,		//The scene has its editor.
	BSL_Failed,		//There's no export to read, or it couldn't be read.
};


//One model's load.  Load reads the model on the thread that calls it, into that thread's object system.  On a loader's
//thread, which is a worker with an object system of its own (StandardLib's ObjectsWorker.h), what's read and made on the
//way is the worker's and is freed there, and the editor that comes of it is adopted by the main object system before
//this, a main object, is pointed at it.  The scene looks at the state to find it loaded.
class CBoardSceneLoad : public CObject
{
CONSTRUCTABLE(CBoardSceneLoad);
DESTRUCTABLE(CBoardSceneLoad);
public:
	Ptr<CMeshEditor>	mpEditor;	//Of the model's mesh, while the state is BSL_Loaded.

protected:
	int32				miModel;
	int32				miState;

public:
	void			Init(int iModel);
	void			Class(void) override;
	void			Free(void) override;

	void			Load(void);
	EBoardSceneLoad	GetState(void);
	void			SetState(EBoardSceneLoad eState);

protected:
	Ptr<CMeshEditor>	LoadSaved(const char* szSaved);
	Ptr<CMeshEditor>	LoadSource(const char* szSaved);
};


//The Small Mainboard's 3D model with the Processor Board in its socket and the boards chosen for its other sockets
//standing in them, turning, in perspective with the eye 30 degrees up from the mainboard's plane.
//
//It's a scene, in millimetres.  The mainboard is an instance whose connections are its own and, as their children, one for
//each socket, at the middle of the floor of its slot.  A board standing in a socket is an instance whose connection is a child
//of the socket's, which stands it up.  Each model is a mesh editor, which every socket holding that board shares.  The scene
//camera follows the raster camera, which turns around the boards with the yaw.
//
//The models are KiCad's VRML exports, which take seconds to read.  One is only needed when it is shown, when it's read, scaled
//to millimetres, cleaned up and simplified to about what a pixel is here, and its mesh saved to Output/Models as a scene file
//(.DRG), from where it loads in under a second from then on, until its export is newer.  They stay in memory, so make this
//once.
//
//Load, called at each tick, has every model that's wanted read on a loader's thread, BOARD_SCENE_LOADERS at a time, and
//takes those that have been: the window goes on drawing, and turning what's there, while they're read, and a board
//appears in its socket when its model is loaded.  The loaders are workers with object systems of their own
//(CBoardSceneLoad), so the objects' locking is on while any model is being read, and off again once none is.  StopLoading
//waits for them: call it before the scene is let go of, and not from a Free, as a loader may be waiting for the objects'
//lock, which a thread in a Free holds.
//
//Where a board goes comes from the KiCad boards: see gasBoardSceneSockets and gasBoardSceneModels.
class CBoardSceneDraw : public CCanvasDraw
{
CONSTRUCTABLE(CBoardSceneDraw);
DESTRUCTABLE(CBoardSceneDraw);
protected:
	Ptr<CImageCanvasDraw>	mpCelDraw;
	Ptr<CImage>				mpImage;		//What the rasteriser draws on.
	Ptr<CScene>				mpScene;
	Ptr<CInstance>			mpMainboard;	//Its connections are the mainboard's and then each socket's.
	CArray<CMeshEditor>		mapModels;		//BOARD_SCENE_NUM_MODELS of them, with empty meshes until loaded.
	CArray<CBoardSceneLoad>	mapLoads;		//Each model's load.
	CWorkerThreadJobs		mcLoaders;		//Whose threads read the models, while mbLoaders.
	bool					mbLoaders;
	bool					mbLockedObjects;	//The loaders switched the objects' locking on, and are to switch it off.
	int						maiShown[BOARD_SCENE_NUM_SOCKETS];	//The model standing in each socket, or -1.
	bool					mabStackShown[BOARD_SCENE_NUM_SOCKETS];	//The board stacked on it is shown too.
	bool					mbMainboardShown;
	CRasterRenderFactory	mcFactory;
	Ptr<CRenderScene>		mpRenderScene;
	Ptr<CRasterFrame>		mpFrame;
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

	void		Load(const char** aszTypes);
	bool		IsLoading(void);
	void		StopLoading(void);
	void		SetBoards(const char** aszTypes);
	void		SetYaw(float32 fYaw);

	static bool	HasMainboardModel(void);

protected:
	bool		Render(void);
	void		InitScene(void);
	void		StartLoad(int iModel);
	bool		StartLoaders(void);
	bool		TakeLoaded(void);
	bool		IsLoaded(int iModel);
	int			FindModel(size uiSocket, const char** aszTypes);
	void		AddStandingBoard(int iModel, size uiSocket, bool bStack);
	void		FitCamera(void);
};


#endif // __BOARD_SCENE_H__
