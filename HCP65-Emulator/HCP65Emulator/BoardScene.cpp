#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/LogString.h"
#include "BaseLib/Timer.h"
#include "BaseLib/Atomic.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ObjectsLock.h"
#include "SupportLib/ImageCel.h"
#include "SupportLib/Mesh.h"
#include "SupportLib/MeshEdgeCollapse.h"
#include "SupportLib/MeshFixReversed.h"
#include "SupportLib/MeshMergeNormals.h"
#include "SupportLib/MeshReader.h"
#include "SupportLib/MeshRemoveDegenerate.h"
#include "SupportLib/MeshWeldCorners.h"
#include "SupportLib/SceneFileMesh.h"
#include "RenderLib/CookedMesh.h"
#include "RasterRenderLib/RasterRenderMesh.h"
#include "WindowLib/Canvas.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65MemoryBoard.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "HCP65Lib/HCP65UART.h"
#include "HCP65Lib/HCP65VIA.h"
#include "HCP65Lib/HCP65PS2.h"
#include "HCP65Lib/HCP65Video.h"
#include "BoardScene.h"


//The renderer's units in a millimetre, which the scene is in.
#define BOARD_SCENE_UNITS_PER_MM	4096

//The error allowed when a card is simplified.  Surfaces move far less than this on average, but with more of it dark
//streaks start to show across its edge connector pads, and its small text and chip legs go.  A millimetre is a little under
//two pixels here.
#define BOARD_SCENE_CARD_MAX_ERROR_MM	0.25

//The mainboard's, which brings it to about 150,000 vertices.  Its pins become small triangles and its sockets' bases jagged,
//but it's what the cards are in front of.
#define BOARD_SCENE_MAINBOARD_MAX_ERROR_MM	6.0

//80 degrees: normals at a corner closer than this merge, and an edge of a box, at 90, stays sharp.  At 90 or more about a
//tenth of the pixels are clearly wrong.
#define BOARD_SCENE_MAX_NORMAL_ANGLE	1.3962634f

//Turned from looking straight down on the board (the camera's start) to 30 degrees up from its plane: 60 degrees, taking
//its back edge away from the eye.
#define BOARD_SCENE_PITCH			-1.04719755f
#define BOARD_SCENE_MARGIN			8

//Where each model's mesh is saved, as a scene file of one mesh with the model's name, in millimetres.  It's used for as long
//as it's at least as new as the model's export.  Nothing else says it's out of date, so delete them when what's done to a
//model changes.
#define BOARD_SCENE_MODEL_DIRECTORY	"Output/Models"
#define BOARD_SCENE_MAINBOARD		0
#define BOARD_SCENE_PROCESSOR		1
#define BOARD_SCENE_FIRST_CARD		2

//The mainboard instance's first connection is its own, and each socket's follows.
#define BOARD_SCENE_FIRST_SOCKET_CONNECTION	1


//A socket on the Small Mainboard: the device it is, -1 for the Processor Board's (MPU0), and the y of its footprint (DEV3,
//say) in the PCB.  A socket's row A is 5.08mm less than that and its row D is at it, along x from 0 to 129.54mm.
struct SBoardSceneSocket
{
	int			iSocket;
	float64		fFootprintY;
};


static const SBoardSceneSocket gasBoardSceneSockets[BOARD_SCENE_NUM_SOCKETS] =
{
	{ -1,	68.58 },
	{ 3,	203.3362 },
	{ 4,	172.8562 },
	{ 16,	5.08 },
	{ 17,	35.56 },
	{ 18,	147.4562 },
	{ 19,	101.6 },
};


//A board's model.  KiCad's exports are in the PCB's coordinates with y negated, but in what units depends on how they were
//exported.  fEdgeY is the y in the PCB of the board's edge connector's footprint, which is the bottom of the board.
//
//A board can have another stacked on it (iStacked, 0 for none): the Video Board's Memory board on its Signals board.
//The stacked board's PCB is laid out in the same coordinates, so it is only moved up, fStackHeight from the board's.
//A stacked board's model has no type of its own: it is only shown on the board it stacks on.
struct SBoardSceneModel
{
	const char*		szType;
	const char*		szFile;
	const char*		szSaved;
	float64			fMillimetresPerUnit;
	float64			fEdgeY;
	float64			fMaxErrorMillimetres;
	int				iStacked;
	float64			fStackHeight;
};


//The Video Board's Memory board stands on its Signals board's pin headers: the Signals board (1.6mm), a header's body
//(2.5mm) and the Memory board's sockets (8.5mm).
#define BOARD_SCENE_VIDEO_MEMORY		9
#define BOARD_SCENE_VIDEO_STACK_HEIGHT	12.6


static const SBoardSceneModel gasBoardSceneModels[BOARD_SCENE_NUM_MODELS] =
{
	{ NULL,						"../../Kicad/Computer/Main/Mainboard Small/Mainboard Small.wrl",	"Mainboard Small",	1000.0,	0.0,	BOARD_SCENE_MAINBOARD_MAX_ERROR_MM,	0,	0.0 },
	{ NULL,						"../../Kicad/Computer/Processor/Processor Board/Processor Board.wrl",	"Processor Board",	1000.0,	5.715,	BOARD_SCENE_CARD_MAX_ERROR_MM,	0,	0.0 },
	{ HCP65_SD_NAME,			"../../Kicad/Computer/SD Card/SD Card Board/SD Card Board.wrl",		"SD Card Board",	2.54,	4.699,	BOARD_SCENE_CARD_MAX_ERROR_MM,	0,	0.0 },
	{ HCP65_UART_NAME,			"../../Kicad/Computer/UART/UART Board/UART Board.wrl",				"UART Board",		2.54,	4.896,	BOARD_SCENE_CARD_MAX_ERROR_MM,	0,	0.0 },
	{ HCP65_AUDIO_NAME,			"../../Kicad/Computer/Audio/Audio Board/Audio Board.wrl",			"Audio Board",		2.54,	4.445,	BOARD_SCENE_CARD_MAX_ERROR_MM,	0,	0.0 },
	{ HCP65_MEMORY_BOARD_NAME,	"../../Kicad/Computer/Memory/Memory Board/Memory Board.wrl",		"Memory Board",		2.54,	4.445,	BOARD_SCENE_CARD_MAX_ERROR_MM,	0,	0.0 },
	{ HCP65_VIA_BOARD_NAME,		"../../Kicad/Computer/VIA/VIA Board/VIA Board.wrl",				"VIA Board",		2.54,	5.08,	BOARD_SCENE_CARD_MAX_ERROR_MM,	0,	0.0 },
	{ HCP65_PS2_BOARD_NAME,		"../../Kicad/Computer/PS2/PS2 Board/PS2 Board.wrl",				"PS2 Board",		2.54,	5.08,	BOARD_SCENE_CARD_MAX_ERROR_MM,	0,	0.0 },
	{ HCP65_VIDEO_BOARD_NAME,	"../../Kicad/Computer/Video/Video Board - Signals/Video Board - Signals.wrl",	"Video Board - Signals",	2.54,	4.445,	BOARD_SCENE_CARD_MAX_ERROR_MM,	BOARD_SCENE_VIDEO_MEMORY,	BOARD_SCENE_VIDEO_STACK_HEIGHT },
	{ NULL,						"../../Kicad/Computer/Video/Video Board - Memory/Video Board - Memory.wrl",		"Video Board - Memory",		2.54,	0.0,	BOARD_SCENE_CARD_MAX_ERROR_MM,	0,	0.0 },
};


//A card's pads are 0.0508mm less in x than the footprint's on the mainboard: they are 2.54mm apart from 7.366mm along a
//footprint at -7.4168mm.
#define BOARD_SCENE_CARD_X			0.0508

//Where the middle of the slot is in the socket's model, from its row D: its walls are 1.9mm apart, for a 1.6mm board.  It
//is 0.17mm past half way to row A, as the model is a little off the footprint.  A board's middle is at z 0 in its model.
#define BOARD_SCENE_SLOT_MIDDLE		2.71

//The height in the model of the floor of a socket's slot, 3.7mm below the bottom of its contacts.
#define BOARD_SCENE_SEAT			8.8

//The room framed above the mainboard for boards standing in its sockets: the tallest is the Processor Board, 161.925mm
//from the seat.
#define BOARD_SCENE_TALLEST_CARD	161.925


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static int32 BoardSceneUnits(float64 fMillimetres)
{
	return (int32)floor(fMillimetres * BOARD_SCENE_UNITS_PER_MM + 0.5);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::Init(Ptr<CWindowTheme> pTheme)
{
	ARGB32	sColour;
	int		i;

	PreInit();

	CCanvasDraw::Init();
	mpCelDraw = OMalloc<CImageCanvasDraw>(Ptr<CImageCel>(), false);
	mapModels.Init();
	mapLoads.Init();
	for (i = 0; i < BOARD_SCENE_NUM_MODELS; i++)
	{
		mapModels.Add(OMalloc<CMeshEditor>(OMalloc<CMesh>()));
		mapLoads.Add(OMalloc<CBoardSceneLoad>(i));
	}
	mbLoaders = false;
	mbLockedObjects = false;
	InitScene();
	for (i = 0; i < BOARD_SCENE_NUM_MODELS; i++)
	{
		mapLoads.Get(i)->SetRenderer(&mcFactory, mpRenderScene->GetRenderer());
	}
	mcRasteriser.Init();
	mcJobs.Init(BOARD_SCENE_WORKERS);
	mcRasteriser.SetJobs(&mcJobs);
	mcTarget.mpiDepths = NULL;
	sColour = pTheme->GetColour(TC_DialogBackground);
	muiBackground = 0xFF000000 | ((uint32)Get8BitRedColour(sColour) << 16) | ((uint32)Get8BitGreenColour(sColour) << 8) | (uint32)Get8BitBlueColour(sColour);
	mfYaw = 0.0f;
	mbFitted = false;
	mbDirty = true;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The mainboard and its sockets, which stay, and a camera.  Nothing is in
// the scene until SetBoards puts the mainboard there.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::InitScene(void)
{
	Ptr<CConnection>	pBoard;
	Ptr<CConnection>	pSocket;
	Ptr<CCamera>		pCamera;
	size				uiSocket;

	mpScene = OMalloc<CScene>();

	mpMainboard = OMalloc<CInstance>();
	mpMainboard->mszName.Set(gasBoardSceneModels[BOARD_SCENE_MAINBOARD].szSaved);
	pBoard = OMalloc<CConnection>();
	mpMainboard->mapConnections.Add(pBoard);
	for (uiSocket = 0; uiSocket < BOARD_SCENE_NUM_SOCKETS; uiSocket++)
	{
		pSocket = OMalloc<CConnection>();
		pSocket->mpParent = pBoard;
		pSocket->msLocalMatrix.pos.Init(0.0f, (float32)(BOARD_SCENE_SLOT_MIDDLE - gasBoardSceneSockets[uiSocket].fFootprintY), (float32)BOARD_SCENE_SEAT, 1.0f);
		mpMainboard->mapConnections.Add(pSocket);
		maiShown[uiSocket] = -1;
		mabStackShown[uiSocket] = false;
	}
	mbMainboardShown = false;

	pCamera = OMalloc<CCamera>();
	pCamera->meCameraType = CAT_Perspective;
	pCamera->mpConnection = OMalloc<CConnection>();
	mpScene->mapCameras.Add(pCamera);
	mpScene->UpdateConnections();

	mcFactory.Init(BOARD_SCENE_UNITS_PER_MM);
	mpRenderScene = OMalloc<CRenderScene>(mpScene, &mcFactory, mcFactory.CreateRenderer());
	mpFrame = OMalloc<CRasterFrame>(mpRenderScene, pCamera);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::Class(void)
{
	CCanvasDraw::Class();
	M_Pointer(mpCelDraw);
	M_Pointer(mpImage);
	M_Pointer(mpScene);
	M_Pointer(mpMainboard);
	M_Embedded(mapModels);
	M_Embedded(mapLoads);
	U_Unknown(CWorkerThreadJobs, mcLoaders);
	U_Bool(mbLoaders);
	U_Bool(mbLockedObjects);
	U_Data(int[BOARD_SCENE_NUM_SOCKETS], maiShown);
	U_Data(bool[BOARD_SCENE_NUM_SOCKETS], mabStackShown);
	U_Bool(mbMainboardShown);
	U_Unknown(CRasterRenderFactory, mcFactory);
	M_Pointer(mpRenderScene);
	M_Pointer(mpFrame);
	U_Data(CRasteriser, mcRasteriser);
	U_Data(CWorkerThreadJobs, mcJobs);
	U_Data(CRasterTarget, mcTarget);
	U_UInt32(muiBackground);
	U_Float32(mfYaw);
	U_Bool(mbFitted);
	U_Bool(mbDirty);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::Free(void)
{
	if (mbLoaders)
	{
		//They can't be waited for here (see the class), and would go on to use what this frees.
		gcLogger.Error2(__METHOD__, " The models' loaders weren't stopped (StopLoading) before the scene was freed.", NULL);
	}
	if (mcTarget.mpiDepths != NULL)
	{
		mcTarget.Kill();
	}
	mcRasteriser.Kill();
	mcJobs.Kill();
	mcFactory.Kill();
	CCanvasDraw::Free();
}


//////////////////////////////////////////////////////////////////////////
//
// A canvas draw isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the Small Mainboard's model is there (the emulator may be run
// from elsewhere): there's nothing to show without it.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::HasMainboardModel(void)
{
	CFileUtil	cFileUtil;

	return cFileUtil.Exists(gasBoardSceneModels[BOARD_SCENE_MAINBOARD].szFile);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SInt32Vec2 CBoardSceneDraw::GetDesiredSize(void)
{
	SInt32Vec2	sSize;

	sSize.Init(BOARD_SCENE_WIDTH, BOARD_SCENE_HEIGHT);
	return sSize;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::Draw(Ptr<CCanvas> pCanvas)
{
	if (!Render())
	{
		return false;
	}
	return mpCelDraw->Draw(pCanvas);
}


//////////////////////////////////////////////////////////////////////////
//
// B, G, R, X is what the rasteriser writes and what the window's canvas
// holds.  It is only drawn again when something has changed.  The scene
// is extracted first, which builds the raster mesh of a model that's new
// to it, so the camera can be fitted once the mainboard's is there.  The
// next extract writes where the camera turned to back to the scene camera.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::Render(void)
{
	CRasterCamera*	pcCamera;

	if (mpImage.IsNull())
	{
		mpImage = OMalloc<CImage>(BOARD_SCENE_WIDTH, BOARD_SCENE_HEIGHT, PT_uint8, IMAGE_DIFFUSE_BLUE, IMAGE_DIFFUSE_GREEN, IMAGE_DIFFUSE_RED, IMAGE_IGNORED, CHANNEL_STOP);
		mpCelDraw->SetCel(OMalloc<CImageCel>(mpImage));
		if (!mcTarget.Init((uint32*)mpImage->GetData(), BOARD_SCENE_WIDTH, BOARD_SCENE_HEIGHT, BOARD_SCENE_WIDTH))
		{
			return false;
		}
		mbDirty = true;
	}

	if (mbDirty)
	{
		mcTarget.Clear(muiBackground);
		mpFrame->Extract(&mcRasteriser, BOARD_SCENE_WIDTH);
		if (!mbFitted)
		{
			FitCamera();
		}
		if (mbFitted)
		{
			pcCamera = mpFrame->GetCamera();
			pcCamera->SetOrbit(mfYaw, BOARD_SCENE_PITCH);
			mcRasteriser.Draw(mpFrame->GetInstances(), mpFrame->NumInstances(), pcCamera, &mcTarget);
		}
		mbDirty = false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Has every model that's wanted and hasn't been tried read: the
// mainboard's, and that of the board in each socket (the Processor
// Board's, or one of aszTypes, for each of HCP65_NUM_DEVICE_SLOTS sockets,
// NULL for empty) with the board stacked on it.  Those read since the
// last call are the scene's from now, for SetBoards to show.  Once none
// is being read the loaders' threads end.  Call it at each tick while
// the scene is shown.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::Load(const char** aszTypes)
{
	size	uiSocket;
	int		iModel;

	StartLoad(BOARD_SCENE_MAINBOARD);
	for (uiSocket = 0; uiSocket < BOARD_SCENE_NUM_SOCKETS; uiSocket++)
	{
		iModel = FindModel(uiSocket, aszTypes);
		if (iModel >= 0)
		{
			StartLoad(iModel);
			if (gasBoardSceneModels[iModel].iStacked)
			{
				StartLoad(gasBoardSceneModels[iModel].iStacked);
			}
		}
	}

	TakeLoaded();
	if (mbLoaders && !IsLoading())
	{
		StopLoading();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void BoardSceneLoadJob(void* pvLoad)
{
	((CBoardSceneLoad*)pvLoad)->Load();
}


//////////////////////////////////////////////////////////////////////////
//
// A model that hasn't been tried is given to the loaders; where they can't
// be started it is read here and now, which can take seconds.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::StartLoad(int iModel)
{
	Ptr<CBoardSceneLoad>	pLoad;
	CFileUtil				cFileUtil;

	pLoad = mapLoads.Get(iModel);
	if (pLoad->GetState() != BSL_NotTried)
	{
		return;
	}

	//Where the loads save what they read: made here, by one thread.
	cFileUtil.TouchDir("Output");
	cFileUtil.TouchDir(BOARD_SCENE_MODEL_DIRECTORY);

	pLoad->SetState(BSL_Loading);
	if (StartLoaders())
	{
		mcLoaders.Run(BoardSceneLoadJob, &pLoad);
	}
	else
	{
		pLoad->Load();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The loaders' threads, each a worker with an object system of its own,
// for which the objects' locking is switched on.  False if it can't be.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::StartLoaders(void)
{
	bool	bLocking;

	if (!mbLoaders)
	{
		bLocking = IsObjectsLocking();
		if (ObjectsLocking(true))
		{
			mcLoaders.Init(BOARD_SCENE_LOADERS, ObjectsWorkerInit, ObjectsThreadKill);
			mbLoaders = true;
			mbLockedObjects = !bLocking;
		}
	}
	return mbLoaders;
}


//////////////////////////////////////////////////////////////////////////
//
// The models loaded since they were last taken are the scene's: true if
// any were.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::TakeLoaded(void)
{
	Ptr<CBoardSceneLoad>	pLoad;
	bool					bTaken;
	size					i;

	bTaken = false;
	for (i = 0; i < mapLoads.NumElements(); i++)
	{
		pLoad = mapLoads.Get(i);
		if (pLoad->GetState() == BSL_Loaded)
		{
			mapModels.Set(i, pLoad->mpEditor);
			pLoad->mpEditor = NULL;
			pLoad->SetState(BSL_Taken);
			bTaken = true;
		}
	}
	return bTaken;
}


//////////////////////////////////////////////////////////////////////////
//
// A model that is to be shown: the render scene is given the render mesh
// its load made of it, if it made one, so that it isn't made as the scene
// is next drawn.  A model's mesh never changes, so it's given each time
// the model is shown again.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::GiveRenderMesh(int iModel)
{
	Ptr<CRenderMesh>	pRenderMesh;

	pRenderMesh = mapLoads.Get(iModel)->mpRenderMesh;
	if (pRenderMesh.IsNotNull())
	{
		mpRenderScene->SetMesh(mapModels.Get(iModel), pRenderMesh);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::IsLoading(void)
{
	size	i;

	for (i = 0; i < mapLoads.NumElements(); i++)
	{
		if (mapLoads.Get(i)->GetState() == BSL_Loading)
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Waits for the models being read, which can take seconds, and ends the
// loaders' threads; what they read is the scene's.  The objects' locking
// goes off again if it was the loaders' doing.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::StopLoading(void)
{
	if (mbLoaders)
	{
		mcLoaders.Kill();
		mbLoaders = false;
		if (mbLockedObjects)
		{
			ObjectsLocking(false);
			mbLockedObjects = false;
		}
		TakeLoaded();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The model of the board in a socket, whether or not it's loaded, or -1
// for an empty socket.
//
//////////////////////////////////////////////////////////////////////////
int CBoardSceneDraw::FindModel(size uiSocket, const char** aszTypes)
{
	const char*	szType;
	int			iModel;

	if (gasBoardSceneSockets[uiSocket].iSocket < 0)
	{
		return BOARD_SCENE_PROCESSOR;
	}

	szType = aszTypes[gasBoardSceneSockets[uiSocket].iSocket];
	if (szType == NULL)
	{
		return -1;
	}
	for (iModel = BOARD_SCENE_FIRST_CARD; iModel < BOARD_SCENE_NUM_MODELS; iModel++)
	{
		if (gasBoardSceneModels[iModel].szType && (strcmp(szType, gasBoardSceneModels[iModel].szType) == 0))
		{
			return iModel;
		}
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::IsLoaded(int iModel)
{
	return mapModels.Get(iModel)->mpMesh->NumFaces() > 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneLoad::Init(int iModel)
{
	PreInit();
	mpEditor = NULL;
	mpRenderMesh = NULL;
	miModel = (int32)iModel;
	miState = BSL_NotTried;
	mpcFactory = NULL;
	mpRenderer = NULL;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
// What the render mesh of the model is made by, and for, when it's
// loaded.  The factory outlives the load.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneLoad::SetRenderer(CRenderFactory* pcFactory, Ptr<CRenderer> pRenderer)
{
	mpcFactory = pcFactory;
	mpRenderer = pRenderer;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneLoad::Class(void)
{
	M_Pointer(mpEditor);
	M_Pointer(mpRenderMesh);
	U_Int32(miModel);
	U_Int32(miState);
	U_Pointer(mpcFactory);
	M_Pointer(mpRenderer);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneLoad::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
EBoardSceneLoad CBoardSceneLoad::GetState(void)
{
	return (EBoardSceneLoad)AtomicLoad(&miState);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneLoad::SetState(EBoardSceneLoad eState)
{
	AtomicStore(&miState, (int32)eState);
}


//////////////////////////////////////////////////////////////////////////
//
// The mesh is saved to a scene file, without materials, as a board only
// has colours.  False if it couldn't be, when the writer leaves no file.
//
//////////////////////////////////////////////////////////////////////////
static bool SaveBoardSceneModel(const char* szSaved, Ptr<CMeshEditor> pEditor)
{
	CSceneFileWriter		cWriter;
	CArraySceneFileIndex	aiMaterials;
	bool					bResult;

	aiMaterials.Init();
	cWriter.Init(szSaved);
	bResult = cWriter.Open();
	if (bResult)
	{
		bResult = WriteSceneFileMesh(&cWriter, &pEditor, &aiMaterials);
		bResult = cWriter.Close() && bResult;
	}
	cWriter.Kill();
	aiMaterials.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// From its saved mesh, if that's at least as new as its export.  Otherwise
// the export is read, its modifiers run, and what comes out is saved for
// next time.  Either way mpEditor is an editor of what's loaded, and
// mpRenderMesh the renderer's mesh of it, main objects: on a loader's
// thread everything was made in the thread's own object system, and the
// two are adopted from it.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneLoad::Load(void)
{
	const SBoardSceneModel*	psModel;
	Ptr<CMeshEditor>		pEditor;
	Ptr<CCookedMesh>		pCooked;
	Ptr<CRenderMesh>		pRenderMesh;
	CChars					szSaved;
	CChars					szTime;
	struct stat				sFile;
	struct stat				sSaved;
	int64					iStart;
	bool					bFromSaved;

	psModel = &gasBoardSceneModels[miModel];
	if (stat(psModel->szFile, &sFile) != 0)
	{
		SetState(BSL_Failed);
		return;
	}

	szSaved.Init(BOARD_SCENE_MODEL_DIRECTORY "/");
	szSaved.Append(psModel->szSaved);
	szSaved.Append(".DRG");

	iStart = GetDebugTickCount();
	bFromSaved = (stat(szSaved.Text(), &sSaved) == 0) && (sSaved.st_mtime >= sFile.st_mtime);
	if (bFromSaved)
	{
		pEditor = LoadSaved(szSaved.Text());
		bFromSaved = pEditor.IsNotNull();
	}
	if (pEditor.IsNull())
	{
		pEditor = LoadSource(szSaved.Text());
	}
	if (pEditor.IsNotNull() && (mpcFactory != NULL))
	{
		pCooked = OMalloc<CCookedMesh>();
		pCooked->Cook(&pEditor->GetOutput());
		pRenderMesh = mpcFactory->CreateMesh(mpRenderer, &pCooked);
		pCooked = NULL;
	}
	if (pEditor.IsNotNull() && (ObjectsOwn() != NULL) && (!ObjectsAdopt(pEditor) || (pRenderMesh.IsNotNull() && !ObjectsAdopt(pRenderMesh))))
	{
		pEditor = NULL;  //Adopt has said why.  A main object can't point at a worker's.
		pRenderMesh = NULL;
	}

	if (pEditor.IsNotNull())
	{
		//The time is put in a string of this thread's own: IntToString's are shared.
		szTime.Init();
		szTime.Append((int32)(GetDebugTickCount() - iStart));
		gcLogger.Info2(__METHOD__, " [", psModel->szSaved, "] from [", bFromSaved ? szSaved.Text() : psModel->szFile, "] in ", szTime.Text(), "ms.", NULL);
		szTime.Kill();
		mpEditor = pEditor;
		mpRenderMesh = pRenderMesh;
		SetState(BSL_Loaded);
	}
	else
	{
		SetState(BSL_Failed);
	}
	szSaved.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// NULL if szSaved can't be read or isn't this model's: one mesh with the
// model's name.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMeshEditor> CBoardSceneLoad::LoadSaved(const char* szSaved)
{
	const SBoardSceneModel*	psModel;
	CSceneFileReader		cReader;
	CArrayMaterial			apMaterials;
	Ptr<CMeshEditor>		pEditor;
	bool					bResult;

	psModel = &gasBoardSceneModels[miModel];
	pEditor = OMalloc<CMeshEditor>(OMalloc<CMesh>());
	apMaterials.Init();
	cReader.Init(szSaved);
	bResult = cReader.Open();
	if (bResult)
	{
		if (cReader.NumMeshes() != 1)
		{
			gcLogger.Warning2(__METHOD__, " [", szSaved, "] doesn't hold 1 mesh.", NULL);
			bResult = false;
		}
		bResult = bResult && ReadSceneFileMesh(&cReader, 0, &pEditor, &apMaterials);
		cReader.Close();
	}
	cReader.Kill();
	apMaterials.Kill();

	if (bResult && !pEditor->mszName.Equals(psModel->szSaved))
	{
		gcLogger.Warning2(__METHOD__, " [", szSaved, "] holds [", pEditor->mszName.Text(), "], not [", psModel->szSaved, "].", NULL);
		bResult = false;
	}
	return bResult ? pEditor : Ptr<CMeshEditor>();
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the model's export, scales it to millimetres, cleans it up and
// simplifies it, and saves what comes out to szSaved.  The editor is of
// that alone, so the export is let go.  Not being able to save it doesn't
// stop it being drawn.  NULL if the export can't be read.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMeshEditor> CBoardSceneLoad::LoadSource(const char* szSaved)
{
	const SBoardSceneModel*	psModel;
	Ptr<CMesh>				pMesh;
	Ptr<CMeshEditor>		pSource;
	Ptr<CMeshEditor>		pEditor;

	psModel = &gasBoardSceneModels[miModel];
	pMesh = OMalloc<CMesh>();
	if (!ReadMesh(pMesh, psModel->szFile, MT_VRML))
	{
		return NULL;
	}

	pSource = OMalloc<CMeshEditor>(pMesh);
	pSource->Scale((float32)psModel->fMillimetresPerUnit);
	pSource->AddModifier<CMeshFixReversed>();
	pSource->AddModifier<CMeshWeldCorners>();
	pSource->AddModifier<CMeshEdgeCollapse>((float32)psModel->fMaxErrorMillimetres);
	pSource->AddModifier<CMeshRemoveDegenerate>();
	pSource->AddModifier<CMeshMergeNormals>(BOARD_SCENE_MAX_NORMAL_ANGLE);

	pEditor = OMalloc<CMeshEditor>(pSource->GetOutput());
	pEditor->mszName.Set(psModel->szSaved);
	pSource = NULL;
	pMesh = NULL;

	if (!SaveBoardSceneModel(szSaved, pEditor))
	{
		gcLogger.Warning2(__METHOD__, " Could not save [", szSaved, "].", NULL);
	}
	return pEditor;
}


//////////////////////////////////////////////////////////////////////////
//
// To the mainboard and the tallest board there could be, wherever it is, so
// that the view stays the same whichever boards are in it.  It waits for the
// mainboard's raster mesh, which the scene's first extract after the
// mainboard is in it builds.  Fit puts the eye back where FitOrbit expects it,
// as the frame set the camera from the scene camera when it first extracted.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::FitCamera(void)
{
	Ptr<CRasterRenderMesh>	pRenderMesh;
	Ptr<CRasterMesh>		pMainboard;
	CRasterCamera*			pcCamera;
	SInt32Vec3				sMax;

	pRenderMesh = mpRenderScene->GetMesh(mapModels.Get(BOARD_SCENE_MAINBOARD));
	if (pRenderMesh.IsNull())
	{
		return;
	}
	pMainboard = pRenderMesh->GetRasterMesh();
	if (pMainboard->NumTriangles() == 0)
	{
		return;
	}

	sMax = pMainboard->msMax;
	sMax.z = BoardSceneUnits(BOARD_SCENE_SEAT + BOARD_SCENE_TALLEST_CARD);
	pcCamera = mpFrame->GetCamera();
	pcCamera->Fit(BOARD_SCENE_WIDTH, BOARD_SCENE_HEIGHT);
	pcCamera->FitOrbit(&pMainboard->msMin, &sMax, BOARD_SCENE_PITCH, BOARD_SCENE_WIDTH, BOARD_SCENE_HEIGHT, BOARD_SCENE_MARGIN);
	mbFitted = true;
}


//////////////////////////////////////////////////////////////////////////
//
// A board standing in a socket has its x along the mainboard's x, its y
// (from the bottom of the board to the top) up the mainboard's z and its z
// (out of its components' side) along the mainboard's -y.  That side faces
// the socket's row D, as its edge connector's row D is on its F.Cu.  Its
// bottom edge sits on the floor of the socket's slot.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::AddStandingBoard(int iModel, size uiSocket, bool bStack)
{
	Ptr<CInstance>		pInstance;
	Ptr<CInstanceMesh>	pInstanceMesh;
	Ptr<CConnection>	pConnection;
	Ptr<CConnection>	pStacked;
	SFloat4x4*			psLocal;
	int					iStacked;

	pConnection = OMalloc<CConnection>();
	pConnection->mpParent = mpMainboard->mapConnections.Get(BOARD_SCENE_FIRST_SOCKET_CONNECTION + uiSocket);
	psLocal = &pConnection->msLocalMatrix;
	psLocal->y.Init(0.0f, 0.0f, 1.0f, 0.0f);
	psLocal->z.Init(0.0f, -1.0f, 0.0f, 0.0f);
	psLocal->pos.Init((float32)BOARD_SCENE_CARD_X, 0.0f, (float32)gasBoardSceneModels[iModel].fEdgeY, 1.0f);

	pInstanceMesh = OMalloc<CInstanceMesh>(mapModels.Get(iModel));
	GiveRenderMesh(iModel);
	pInstanceMesh->mapConnections.Add(pConnection);
	pInstance = OMalloc<CInstance>();
	pInstance->mszName.Set(gasBoardSceneModels[iModel].szSaved);
	pInstance->mapConnections.Add(pConnection);
	pInstance->mapMeshes.Add(pInstanceMesh);

	//The stacked board, moved up from this one in the PCB's coordinates.
	if (bStack)
	{
		iStacked = gasBoardSceneModels[iModel].iStacked;
		pStacked = OMalloc<CConnection>();
		pStacked->mpParent = pConnection;
		pStacked->msLocalMatrix.pos.Init(0.0f, 0.0f, (float32)gasBoardSceneModels[iModel].fStackHeight, 1.0f);

		pInstanceMesh = OMalloc<CInstanceMesh>(mapModels.Get(iStacked));
		GiveRenderMesh(iStacked);
		pInstanceMesh->mapConnections.Add(pStacked);
		pInstance->mapConnections.Add(pStacked);
		pInstance->mapMeshes.Add(pInstanceMesh);
	}
	mpScene->mapInstances.Add(pInstance);
}


//////////////////////////////////////////////////////////////////////////
//
// The mainboard, and the board of each socket standing in it, each if its
// model is loaded: the Processor Board in its socket and in each of the
// others the board of aszTypes (HCP65_NUM_DEVICE_SLOTS of them, NULL for
// empty).  The scene is only made again when that's changed.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::SetBoards(const char** aszTypes)
{
	Ptr<CInstanceMesh>	pInstanceMesh;
	int					aiShown[BOARD_SCENE_NUM_SOCKETS];
	bool				abStackShown[BOARD_SCENE_NUM_SOCKETS];
	bool				bMainboard;
	bool				bChanged;
	size				uiSocket;
	int					iModel;

	bMainboard = IsLoaded(BOARD_SCENE_MAINBOARD);
	bChanged = bMainboard != mbMainboardShown;
	for (uiSocket = 0; uiSocket < BOARD_SCENE_NUM_SOCKETS; uiSocket++)
	{
		iModel = FindModel(uiSocket, aszTypes);
		aiShown[uiSocket] = ((iModel >= 0) && IsLoaded(iModel)) ? iModel : -1;
		abStackShown[uiSocket] = (aiShown[uiSocket] >= 0) && gasBoardSceneModels[iModel].iStacked && IsLoaded(gasBoardSceneModels[iModel].iStacked);
		bChanged |= (aiShown[uiSocket] != maiShown[uiSocket]) || (abStackShown[uiSocket] != mabStackShown[uiSocket]);
	}
	if (!bChanged)
	{
		return;
	}

	mpScene->mapInstances.RemoveAll();
	mpMainboard->mapMeshes.RemoveAll();
	if (bMainboard)
	{
		pInstanceMesh = OMalloc<CInstanceMesh>(mapModels.Get(BOARD_SCENE_MAINBOARD));
		GiveRenderMesh(BOARD_SCENE_MAINBOARD);
		pInstanceMesh->mapConnections.Add(mpMainboard->mapConnections.Get(0));
		mpMainboard->mapMeshes.Add(pInstanceMesh);
	}
	mpScene->mapInstances.Add(mpMainboard);
	mbMainboardShown = bMainboard;

	for (uiSocket = 0; uiSocket < BOARD_SCENE_NUM_SOCKETS; uiSocket++)
	{
		maiShown[uiSocket] = aiShown[uiSocket];
		mabStackShown[uiSocket] = abStackShown[uiSocket];
		if (aiShown[uiSocket] >= 0)
		{
			AddStandingBoard(aiShown[uiSocket], uiSocket, abStackShown[uiSocket]);
		}
	}
	mpScene->UpdateConnections();
	mbDirty = true;
}


//////////////////////////////////////////////////////////////////////////
//
// How far it has turned, in radians.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::SetYaw(float32 fYaw)
{
	mfYaw = fYaw;
	mbDirty = true;
}
