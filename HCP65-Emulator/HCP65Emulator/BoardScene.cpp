#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/LogString.h"
#include "BaseLib/Timer.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "SupportLib/ImageCel.h"
#include "SupportLib/Mesh.h"
#include "SupportLib/MeshEdgeCollapse.h"
#include "SupportLib/MeshFixReversed.h"
#include "SupportLib/MeshMergeNormals.h"
#include "SupportLib/MeshModifierStack.h"
#include "SupportLib/MeshReader.h"
#include "SupportLib/MeshWeldCorners.h"
#include "SupportLib/SceneFileMesh.h"
#include "RasterRenderLib/RasterMeshBuild.h"
#include "WindowLib/Canvas.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65MemoryBoard.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "HCP65Lib/HCP65UART.h"
#include "BoardScene.h"


//All of a scene is in one scale, whatever its models' files are in.
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

//Where each model's mesh is saved, as a scene file of one mesh with the model's name.  It's used for as long as it's at least
//as new as the model's export.  Nothing else says it's out of date, so delete them when what's done to a model changes.
#define BOARD_SCENE_MODEL_DIRECTORY	"Output/Models"
#define BOARD_SCENE_MAINBOARD		0
#define BOARD_SCENE_PROCESSOR		1
#define BOARD_SCENE_FIRST_CARD		2


//A socket on the Small Mainboard: the device it is and the y of its footprint (DEV3, say) in the PCB.  A socket's row A is
//5.08mm less than that and its row D is at it, along x from 0 to 129.54mm.
struct SBoardSceneSocket
{
	int			iSocket;
	float64		fFootprintY;
};


static const SBoardSceneSocket gasBoardSceneSockets[] =
{
	{ 3,	203.3362 },
	{ 4,	172.8562 },
	{ 16,	5.08 },
	{ 17,	35.56 },
	{ 18,	147.4562 },
	{ 19,	101.6 },
};


//The y of the Processor Board's socket's footprint (MPU0), which is like the others'.
#define BOARD_SCENE_PROCESSOR_FOOTPRINT_Y	68.58


//A board's model.  KiCad's exports are in the PCB's coordinates with y negated, but in what units depends on how they were
//exported.  fEdgeY is the y in the PCB of the board's edge connector's footprint, which is the bottom of the board.
struct SBoardSceneModel
{
	const char*		szType;
	const char*		szFile;
	const char*		szSaved;
	float64			fMillimetresPerUnit;
	float64			fEdgeY;
	float64			fMaxErrorMillimetres;
};


static const SBoardSceneModel gasBoardSceneModels[BOARD_SCENE_NUM_MODELS] =
{
	{ NULL,						"../../Kicad/Computer/Main/Mainboard Small/Mainboard Small.wrl",	"Mainboard Small",	1000.0,	0.0,	BOARD_SCENE_MAINBOARD_MAX_ERROR_MM },
	{ NULL,						"../../Kicad/Computer/Processor/Processor Board/Processor Board.wrl",	"Processor Board",	1000.0,	5.715,	BOARD_SCENE_CARD_MAX_ERROR_MM },
	{ HCP65_SD_NAME,			"../../Kicad/Computer/SD Card/SD Card Board/SD Card Board.wrl",		"SD Card Board",	2.54,	4.699,	BOARD_SCENE_CARD_MAX_ERROR_MM },
	{ HCP65_UART_NAME,			"../../Kicad/Computer/UART/UART Board/UART Board.wrl",				"UART Board",		2.54,	4.896,	BOARD_SCENE_CARD_MAX_ERROR_MM },
	{ HCP65_AUDIO_NAME,			"../../Kicad/Computer/Audio/Audio Board/Audio Board.wrl",			"Audio Board",		2.54,	4.445,	BOARD_SCENE_CARD_MAX_ERROR_MM },
	{ HCP65_MEMORY_BOARD_NAME,	"../../Kicad/Computer/Memory/Memory Board/Memory Board.wrl",		"Memory Board",		2.54,	4.445,	BOARD_SCENE_CARD_MAX_ERROR_MM },
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
	for (i = 0; i < BOARD_SCENE_NUM_MODELS; i++)
	{
		mapModels.Add(OMalloc<CRasterMesh>());
		mabTried[i] = false;
	}
	muiNumInstances = 0;
	mcCamera.Init();
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
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::Class(void)
{
	CCanvasDraw::Class();
	M_Pointer(mpCelDraw);
	M_Pointer(mpImage);
	M_Embedded(mapModels);
	U_Data(bool[BOARD_SCENE_NUM_MODELS], mabTried);
	U_Data(SRasterInstance[BOARD_SCENE_NUM_INSTANCES], masInstances);
	U_Data(size, muiNumInstances);
	U_Data(CRasterCamera, mcCamera);
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
	if (mcTarget.mpiDepths != NULL)
	{
		mcTarget.Kill();
	}
	mcRasteriser.Kill();
	mcJobs.Kill();
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
// B, G, R, X is what the rasteriser writes and what the window's canvas
// holds.  It is only drawn again when something has changed.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::Draw(Ptr<CCanvas> pCanvas)
{
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
		if (mbFitted)
		{
			mcCamera.SetOrbit(mfYaw, BOARD_SCENE_PITCH);
			mcRasteriser.Draw(masInstances, muiNumInstances, &mcCamera, &mcTarget);
		}
		mbDirty = false;
	}
	return mpCelDraw->Draw(pCanvas);
}


//////////////////////////////////////////////////////////////////////////
//
// Loads the mainboard's model, the Processor Board's, or else one of the
// boards in aszTypes (for each of HCP65_NUM_DEVICE_SLOTS sockets, NULL for
// empty), that hasn't been tried.  False if there are none to load.  It can
// take seconds.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::LoadNext(const char** aszTypes)
{
	size	uiSocket;
	int		iModel;

	for (iModel = 0; iModel < BOARD_SCENE_FIRST_CARD; iModel++)
	{
		if (!mabTried[iModel])
		{
			LoadModel(iModel);
			return true;
		}
	}
	for (uiSocket = 0; uiSocket < sizeof(gasBoardSceneSockets) / sizeof(SBoardSceneSocket); uiSocket++)
	{
		for (iModel = BOARD_SCENE_FIRST_CARD; iModel < BOARD_SCENE_NUM_MODELS; iModel++)
		{
			if (!mabTried[iModel] && aszTypes[gasBoardSceneSockets[uiSocket].iSocket] && (strcmp(aszTypes[gasBoardSceneSockets[uiSocket].iSocket], gasBoardSceneModels[iModel].szType) == 0))
			{
				LoadModel(iModel);
				return true;
			}
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// The mesh is saved to a scene file once its modifiers have run, without
// materials, as a board only has colours.  False if it couldn't be, when the
// writer leaves no file.
//
//////////////////////////////////////////////////////////////////////////
static bool SaveBoardSceneModel(const char* szSaved, const char* szName, Ptr<CMesh> pMesh)
{
	CSceneFileWriter		cWriter;
	CArraySceneFileIndex	aiMaterials;
	Ptr<CMeshEditor>		pEditor;
	bool					bResult;

	pEditor = OMalloc<CMeshEditor>(pMesh);
	pEditor->mszName.Set(szName);
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
	pEditor = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// From its saved mesh, if that's at least as new as its export.  Otherwise
// the export is read, its modifiers run, and what comes out is saved for next
// time.  Either way the mesh is built once and for all, at the one scale all
// the models are at.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::LoadModel(int iModel)
{
	const SBoardSceneModel*	psModel;
	CFileUtil				cFileUtil;
	CChars					szSaved;
	struct stat				sFile;
	struct stat				sSaved;
	int64					iStart;
	bool					bFromSaved;
	bool					bResult;

	psModel = &gasBoardSceneModels[iModel];
	mabTried[iModel] = true;
	if (stat(psModel->szFile, &sFile) != 0)
	{
		return false;
	}

	cFileUtil.TouchDir("Output");
	cFileUtil.TouchDir(BOARD_SCENE_MODEL_DIRECTORY);
	szSaved.Init(BOARD_SCENE_MODEL_DIRECTORY "/");
	szSaved.Append(psModel->szSaved);
	szSaved.Append(".DRG");

	iStart = GetDebugTickCount();
	bFromSaved = (stat(szSaved.Text(), &sSaved) == 0) && (sSaved.st_mtime >= sFile.st_mtime) && LoadSavedModel(iModel, szSaved.Text());
	bResult = bFromSaved || LoadSourceModel(iModel, szSaved.Text());
	if (bResult)
	{
		gcLogger.Info2(__METHOD__, " [", psModel->szSaved, "] from [", bFromSaved ? szSaved.Text() : psModel->szFile, "] in ", IntToString((int32)(GetDebugTickCount() - iStart)), "ms.", NULL);
	}
	szSaved.Kill();

	if (bResult && (iModel == BOARD_SCENE_MAINBOARD))
	{
		FitCamera();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// False if szSaved can't be read or isn't this model's: one mesh with the
// model's name.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::LoadSavedModel(int iModel, const char* szSaved)
{
	const SBoardSceneModel*	psModel;
	CSceneFileReader		cReader;
	CArrayMaterial			apMaterials;
	Ptr<CMeshEditor>		pEditor;
	bool					bResult;

	psModel = &gasBoardSceneModels[iModel];
	pEditor = OMalloc<CMeshEditor>(OMalloc<CMesh>());
	apMaterials.Init();
	cReader.Init(szSaved);
	bResult = cReader.Open();
	if (bResult)
	{
		if (cReader.NumMeshes() != 1)
		{
			gcLogger.Warning2(__METHOD__, " [", szSaved, "] holds ", IntToString((int32)cReader.NumMeshes()), " meshes, not 1.", NULL);
			bResult = false;
		}
		bResult = bResult && ReadSceneFileMesh(&cReader, 0, &pEditor, &apMaterials);
		cReader.Close();
	}
	cReader.Kill();

	if (bResult && !pEditor->mszName.Equals(psModel->szSaved))
	{
		gcLogger.Warning2(__METHOD__, " [", szSaved, "] holds [", pEditor->mszName.Text(), "], not [", psModel->szSaved, "].", NULL);
		bResult = false;
	}
	if (bResult)
	{
		BuildModel(iModel, &pEditor->mpMesh);
	}
	pEditor = NULL;
	apMaterials.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the model's export, cleans it up and simplifies it, builds what comes
// out, and saves it to szSaved.  Not being able to save it doesn't stop it
// being drawn.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardSceneDraw::LoadSourceModel(int iModel, const char* szSaved)
{
	const SBoardSceneModel*	psModel;
	CMeshModifierStack		cModifiers;
	Ptr<CMesh>				pMesh;
	Ptr<CMesh>				pOutput;

	psModel = &gasBoardSceneModels[iModel];
	pMesh = OMalloc<CMesh>();
	if (!ReadMesh(pMesh, psModel->szFile, MT_VRML))
	{
		return false;
	}

	cModifiers.Init(pMesh);
	cModifiers.AddModifier<CMeshFixReversed>()->Init();
	cModifiers.AddModifier<CMeshWeldCorners>()->Init();
	cModifiers.AddModifier<CMeshEdgeCollapse>()->Init((float32)(psModel->fMaxErrorMillimetres / psModel->fMillimetresPerUnit));
	cModifiers.AddModifier<CMeshMergeNormals>()->Init(BOARD_SCENE_MAX_NORMAL_ANGLE);
	pOutput = cModifiers.GetOutput();
	BuildModel(iModel, &pOutput);
	if (!SaveBoardSceneModel(szSaved, psModel->szSaved, pOutput))
	{
		gcLogger.Warning2(__METHOD__, " Could not save [", szSaved, "].", NULL);
	}
	pOutput = NULL;
	cModifiers.Kill();
	pMesh = NULL;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::BuildModel(int iModel, CMesh* pcMesh)
{
	Ptr<CRasterMesh>	pModel;

	pModel = mapModels.Get(iModel);
	BuildRasterMesh(&pModel, pcMesh, BoardSceneUnits(gasBoardSceneModels[iModel].fMillimetresPerUnit));
	pModel->RemoveDegenerate();
}


//////////////////////////////////////////////////////////////////////////
//
// To the mainboard and the tallest board there could be, wherever it is, so
// that the view stays the same whichever boards are in it.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::FitCamera(void)
{
	Ptr<CRasterMesh>	pMainboard;
	SInt32Vec3			sMax;

	pMainboard = mapModels.Get(BOARD_SCENE_MAINBOARD);
	sMax = pMainboard->msMax;
	sMax.z = BoardSceneUnits(BOARD_SCENE_SEAT + BOARD_SCENE_TALLEST_CARD);
	mcCamera.FitOrbit(&pMainboard->msMin, &sMax, BOARD_SCENE_PITCH, BOARD_SCENE_WIDTH, BOARD_SCENE_HEIGHT, BOARD_SCENE_MARGIN);
	mbFitted = true;
}


//////////////////////////////////////////////////////////////////////////
//
// A model that's standing has its x along the mainboard's x, its y (from
// the bottom of the board to the top) up the mainboard's z and its z (out of
// its components' side) along the mainboard's -y.  That side faces the socket's
// row D, as its edge connector's row D is on its F.Cu.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::AddInstance(int iModel, SInt32Vec3* psOffset, bool bStanding)
{
	SRasterInstance*	psInstance;

	psInstance = &masInstances[muiNumInstances];
	muiNumInstances++;
	psInstance->pcMesh = &mapModels.Get(iModel);
	psInstance->sOffset = *psOffset;
	psInstance->asAxes[0].Init(1 << RASTER_AXIS_BITS, 0, 0);
	if (bStanding)
	{
		psInstance->asAxes[1].Init(0, 0, -(1 << RASTER_AXIS_BITS));
		psInstance->asAxes[2].Init(0, 1 << RASTER_AXIS_BITS, 0);
	}
	else
	{
		psInstance->asAxes[1].Init(0, 1 << RASTER_AXIS_BITS, 0);
		psInstance->asAxes[2].Init(0, 0, 1 << RASTER_AXIS_BITS);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// iModel in the socket whose footprint is at y fFootprintY.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::AddStandingBoard(int iModel, float64 fFootprintY)
{
	SInt32Vec3	sOffset;

	sOffset.Init(BoardSceneUnits(BOARD_SCENE_CARD_X),
				 BoardSceneUnits(BOARD_SCENE_SLOT_MIDDLE - fFootprintY),
				 BoardSceneUnits(BOARD_SCENE_SEAT + gasBoardSceneModels[iModel].fEdgeY));
	AddInstance(iModel, &sOffset, true);
}


//////////////////////////////////////////////////////////////////////////
//
// The mainboard, the Processor Board in its socket, and in each of the other
// sockets the board of aszTypes (HCP65_NUM_DEVICE_SLOTS of them, NULL for
// empty), each if its model is loaded.
//
//////////////////////////////////////////////////////////////////////////
void CBoardSceneDraw::SetBoards(const char** aszTypes)
{
	const SBoardSceneSocket*	psSocket;
	SInt32Vec3					sOffset;
	const char*					szType;
	size						uiSocket;
	int							iModel;

	muiNumInstances = 0;
	if (mapModels.Get(BOARD_SCENE_MAINBOARD)->NumTriangles() > 0)
	{
		sOffset.Init(0, 0, 0);
		AddInstance(BOARD_SCENE_MAINBOARD, &sOffset, false);
	}
	if (mapModels.Get(BOARD_SCENE_PROCESSOR)->NumTriangles() > 0)
	{
		AddStandingBoard(BOARD_SCENE_PROCESSOR, BOARD_SCENE_PROCESSOR_FOOTPRINT_Y);
	}
	for (uiSocket = 0; uiSocket < sizeof(gasBoardSceneSockets) / sizeof(SBoardSceneSocket); uiSocket++)
	{
		psSocket = &gasBoardSceneSockets[uiSocket];
		szType = aszTypes[psSocket->iSocket];
		for (iModel = BOARD_SCENE_FIRST_CARD; szType && (iModel < BOARD_SCENE_NUM_MODELS); iModel++)
		{
			if ((strcmp(szType, gasBoardSceneModels[iModel].szType) == 0) && (mapModels.Get(iModel)->NumTriangles() > 0))
			{
				AddStandingBoard(iModel, psSocket->fFootprintY);
			}
		}
	}
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
