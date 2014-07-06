/**********************************************************************
 *<
    FILE: Unreal3DExport.cpp

    DESCRIPTION:    Unreal Engine 2 .3d Exporter for 3dsmax8

    CREATED BY:     Roman Switch` Dzieciol

    HISTORY: 

 *> Copyright (c) 2007, All Rights Reserved.
 **********************************************************************/

#include <math.h>
#include "Unreal3DExport.h"
#include "U3DFormat.h"
#include "decomp.h"
#include "utilapi.h"
#include <inode.h> 
#include <notetrck.h> 

#include "IGame.h"
#include "IGameObject.h"
#include "IGameProperty.h"
#include "IGameControl.h"
#include "IGameModifier.h"
#include "IConversionManager.h"
#include "IGameError.h"
#include "IGameFX.h"
#include <IGameMaterial.h> 

#define Unreal3DExport_CLASS_ID Class_ID(0x4f803aa9, 0x95911a2e)

// NOTE: Update anytime the CFG file changes
#define CFG_VERSION 0x01

struct sMaterial
{
    IGameMaterial* Mat;
    TSTR* Tex;
    int Flags;
    bool bSkin;
    static sMaterial DefaultMaterial;

    sMaterial(IGameMaterial* cmat=NULL, TSTR* ctex=NULL, int cflags=0)
    : Mat(cmat), Tex(ctex), Flags(cflags)
    {
    }
};

sMaterial sMaterial::DefaultMaterial = sMaterial();


class Unreal3DExport : public SceneExport 
{
public:
    static HWND         hParams;

    // 3DXI
    Interface *         pInt;
    IGameScene *        pScene;

    // Files
    FILE*               fMesh;
    FILE*               fAnim;
    FILE*               fLog;
    FILE*               fScript;

    // Scene data
    Tab<IGameNode*>     Nodes;
    Tab<IGameNode*>     TrackedNodes;
    Tab<FMeshVert>      Verts;
    Tab<FJSMeshTri>     Tris;
    Tab<Point3>         Points;
    Tab<NoteTrack*>     NoteTracks;
    Tab<sMaterial>      Materials;
    
    int                 NodeIdx;
    int                 NodeCount;
    int                 VertsPerFrame;
    TSTR                SeqName;
    int                 SeqFrame;
    int                 FrameStart;
    int                 FrameEnd;
    int                 FrameCount;
    
    // Global options
    bool                bExportSelected;
    bool                bShowPrompts;
    bool                bIgnoreHidden;
    bool                bMaxResolution;

    // Progress Bar
    float               Progress;
    TSTR                ProgressMsg;

    // Optimization
    Point3              OptScale;
    Point3              OptOffset;
    Point3              OptRot;

    // File names
    TSTR                FilePath;
    TSTR                FileName;
    TSTR                FileExt;
    TSTR                ModelFileName;
    TSTR                AnimFileName;
    TSTR                ScriptFileName;

    // File Headers
    FJSDataHeader       hData;
    FJSAnivHeader       hAnim;

    
public:
    int             ExtCount();                 // Number of extensions supported
    const TCHAR *   Ext(int n);                 // Extension #n (i.e. "3DS")
    const TCHAR *   LongDesc();                 // Long ASCII description (i.e. "Autodesk 3D Studio File")
    const TCHAR *   ShortDesc();                // Short ASCII description (i.e. "3D Studio")
    const TCHAR *   AuthorName();               // ASCII Author name
    const TCHAR *   CopyrightMessage();         // ASCII Copyright message
    const TCHAR *   OtherMessage1();            // Other message #1
    const TCHAR *   OtherMessage2();            // Other message #2
    unsigned int    Version();                  // Version number * 100 (i.e. v3.01 = 301)
    void            ShowAbout(HWND hWnd);       // Show DLL's "About..." box

    BOOL            SupportsOptions(int ext, DWORD options);
    int             DoExport(const TCHAR *name,ExpInterface *ei,Interface *i, BOOL bShowPrompts=FALSE, DWORD options=0);

public:
    Unreal3DExport();
    ~Unreal3DExport();      

protected:
    // Custom
    void CheckCancel();
    void ExportNode( IGameNode * child);
    void RegisterMaterial( IGameNode* node, IGameMesh* mesh, FaceEx* f, FJSMeshTri* tri );
    void SortMaterials();
    void Init();
    void GetTris();
    void GetAnim();
    void Prepare();
    void WriteScript();
    void WriteModel();
    void WriteTracking();
    void ShowSummary();

    // Config
    BOOL ReadConfig();
    void WriteConfig();
    TSTR GetCfgFileName();

};



class Unreal3DExportClassDesc : public ClassDesc2 {
    public:
    int             IsPublic() { return TRUE; }
    void *          Create(BOOL loading = FALSE) { return new Unreal3DExport(); }
    const TCHAR *   ClassName() { return GetString(IDS_CLASS_NAME); }
    SClass_ID       SuperClassID() { return SCENE_EXPORT_CLASS_ID; }
    Class_ID        ClassID() { return Unreal3DExport_CLASS_ID; }
    const TCHAR*    Category() { return GetString(IDS_CATEGORY); }

    const TCHAR*    InternalName() { return _T("Unreal3DExport"); } // returns fixed parsable name (scripter-visible name)
    HINSTANCE       HInstance() { return hInstance; }                   // returns owning module handle
    

};

static Unreal3DExportClassDesc Unreal3DExportDesc;
ClassDesc2* GetUnreal3DExportDesc() { return &Unreal3DExportDesc; }


#include "U3DUtil.h"


//--- Unreal3DExport -------------------------------------------------------
Unreal3DExport::Unreal3DExport()
: pInt(NULL)
, pScene(NULL)
, fMesh(NULL)
, fAnim(NULL)
, fLog(NULL)
, fScript(NULL)
, bExportSelected(false)
, bShowPrompts(false)
, bIgnoreHidden(false)
, bMaxResolution(true)
, NodeIdx(0)
, NodeCount(0)
, VertsPerFrame(0)
, SeqFrame(0)
, FrameStart(0)
, FrameEnd(0)
, FrameCount(0)
, Progress(0)
, OptScale(1,1,1)
, OptOffset(0,0,0)
, OptRot(0,0,0)
, hData(FJSDataHeader())
, hAnim(FJSAnivHeader())
{
}

Unreal3DExport::~Unreal3DExport() 
{
}

BOOL CALLBACK Unreal3DExportOptionsDlgProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam) 
{
	static Unreal3DExport *imp = NULL;

	switch(message) {
		case WM_INITDIALOG:
			imp = (Unreal3DExport *)lParam;
			CenterWindow(hWnd,GetParent(hWnd));

            SetDlgItemInt(hWnd, IDC_EDIT_H, UnrealCoords.rotation, FALSE );
            SetDlgItemInt(hWnd, IDC_EDIT_X, UnrealCoords.xAxis, FALSE );
            SetDlgItemInt(hWnd, IDC_EDIT_Y, UnrealCoords.yAxis, FALSE );
            SetDlgItemInt(hWnd, IDC_EDIT_Z, UnrealCoords.zAxis, FALSE );
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam)) 
            {
		        case IDOK:
                    UnrealCoords.rotation = GetDlgItemInt(hWnd, IDC_EDIT_H, NULL, FALSE );
                    UnrealCoords.xAxis = GetDlgItemInt(hWnd, IDC_EDIT_X, NULL, FALSE );
                    UnrealCoords.yAxis = GetDlgItemInt(hWnd, IDC_EDIT_Y, NULL, FALSE );
                    UnrealCoords.zAxis = GetDlgItemInt(hWnd, IDC_EDIT_Z, NULL, FALSE );
			        EndDialog(hWnd, 1);
			        break;

				case IDCANCEL:
					EndDialog(hWnd,0);
					break;
		    }
		default:
			return FALSE;
	}
	return TRUE;
}

int Unreal3DExport::DoExport( const TCHAR *name, ExpInterface *ei, Interface *i, BOOL suppressPrompts, DWORD options )
{
    int Result = FALSE;

    // Set a global prompt display switch
    bShowPrompts = suppressPrompts ? false : true;
    bExportSelected = (options & SCENE_EXPORT_SELECTED) ? true : false;
    
    // Get file names
    SplitFilename(TSTR(name), &FilePath, &FileName, &FileExt);

    if( MatchPattern(FileName,TSTR(_T("*_d")),TRUE)
    ||  MatchPattern(FileName,TSTR(_T("*_a")),TRUE) )
    {
        FileName = FileName.Substr(0,FileName.length()-2);
    }

    ModelFileName = FilePath + _T("\\") + FileName + TSTR(_T("_d")) + FileExt;
    AnimFileName = FilePath + _T("\\") + FileName + TSTR(_T("_a")) + FileExt;
    ScriptFileName = FilePath + _T("\\") + FileName + TSTR(_T("_rc.uc"));


    // Open Log
    fLog = _tfopen(FilePath + _T("\\") + FileName + _T(".log") ,_T("wb"));

    // Init
    pInt = GetCOREInterface();
    pInt->ProgressStart( GetString(IDS_INFO_INIT), TRUE, fn, this);
    Progress += U3D_PROGRESS_INIT;

    try 
    {
        MyErrorProc pErrorProc;
        SetErrorCallBack(&pErrorProc);

        ReadConfig();


        //if(bShowPrompts)
        /*DialogBoxParam(hInstance, 
                MAKEINTRESOURCE(IDD_PANEL), 
                GetActiveWindow(), 
                Unreal3DExportOptionsDlgProc, (LPARAM)this);*/

	    //if(showPrompts) 
	    {
		    // Prompt the user with our dialogbox, and get all the options.
		    if(!DialogBoxParam(hInstance, 
				MAKEINTRESOURCE(IDD_PANEL), 
				GetActiveWindow(), 
				Unreal3DExportOptionsDlgProc, (LPARAM)this)) 
            {
			    throw CancelException();
		    }
	    }

        // Enumerate interesting nodes
        Init();

        // Fetch data from nodes
        GetTris();
        GetAnim();

        // Prepare data for writing
        Prepare();     

        // Write to files
        WriteScript();
        WriteModel();   
        WriteTracking();

        // Show optional summary
        ShowSummary();

        WriteConfig();

        Result = IMPEXP_SUCCESS;
    }
    catch( CancelException& )
    {
        Result = IMPEXP_CANCEL;
    }
    catch( MAXException& e )
    {
        if( bShowPrompts && !e.message.isNull() )
        {
            MaxMsgBox(pInt->GetMAXHWnd(),e.message,ShortDesc(),MB_OK|MB_ICONERROR);
        }

        Result = IMPEXP_FAIL;
    }

    // Release scene
    if( pScene != NULL )
    {
        pScene->ReleaseIGame();
        pScene = NULL;
    }

    // Close files
    fclosen(fMesh);
    fclosen(fAnim);
    fclosen(fLog);
    fclosen(fScript);
    
    // Return to MAX
    pInt->ProgressEnd();  
    return Result;
}

void Unreal3DExport::ExportNode( IGameNode * child )
{
    DebugPrint( _T("ExportNode: %s\n"), child->GetName() );
    CheckCancel();

    ProgressMsg.printf(GetString(IDS_INFO_ENUM_OBJ),NodeIdx,NodeCount,TSTR(child->GetName()));
    pInt->ProgressUpdate(Progress+((float)NodeIdx/NodeCount*U3D_PROGRESS_ENUM), FALSE, ProgressMsg.data());
    ++NodeIdx;
    
    if( child->IsGroupOwner() )
    {
        // do nothing
    }
    else
    {
        IGameObject * obj = child->GetIGameObject();

        switch(obj->GetIGameType())
        {
            case IGameObject::IGAME_MESH:
            { 
                if( !bIgnoreHidden || !child->IsNodeHidden() )
                {
                    Nodes.Append(1,&child);
                }
                break;
            }
			case IGameObject::IGAME_HELPER:
            {
                if( !bIgnoreHidden || !child->IsNodeHidden() )
                {
                    TrackedNodes.Append(1,&child);
                }
            }
            break;
        }

        child->ReleaseIGameObject();
    }   
    
    for( int i=0; i<child->GetChildCount(); ++i )
    {
        IGameNode * n = child->GetNodeChild(i);
        ExportNode(n);
    }
}


void Unreal3DExport::CheckCancel()
{
    if( pInt->GetCancel() ) 
    {
        if( !bShowPrompts )
            throw CancelException();

        switch(MessageBox(pInt->GetMAXHWnd(), GetString(IDS_CANCEL_Q), GetString(IDS_CANCEL_C), MB_ICONQUESTION | MB_YESNO)) 
        {
            case IDYES:
                throw CancelException();

            case IDNO:
                pInt->SetCancel(FALSE);
        }
    }
}

void Unreal3DExport::RegisterMaterial( IGameNode* node, IGameMesh* mesh, FaceEx* f, FJSMeshTri* tri )
{
    tri->TextureNum = f->matID;

    int matid = f->matID;

    IGameMaterial* mat = mesh->GetMaterialFromFace(f);
    if( mat )
    {
        Tab<TSTR*> tokens = TokStr(mat->GetMaterialName(),_T(" \t,;"));
        for( int i=0; i!=tokens.Count(); ++i )
        {
            if( MatchPattern(*tokens[i],TSTR(_T("F=*")),TRUE) )
            {
                SplitStr(*tokens[i],_T('='));
                tri->Flags = static_cast<byte>(_ttoi(tokens[i]->data()));
            }
        }
        tokens.Delete(0,tokens.Count());

        if( Materials.Count() <= matid )
        {
            Materials.Append(matid-Materials.Count()+1,&sMaterial::DefaultMaterial);
        }
        
        if( Materials[matid].Mat != mat )
        {
            Materials[matid].Mat = mat;
        }

        /*for( int i=0; i!=mat->GetSubMaterialCount(); ++i )
        {
            IGameMaterial* sub = mat->GetSubMaterial(i);
            if( sub )
            {
                TSTR matname = sub->GetMaterialName();
                int subid = mat->GetMaterialID(i);
                int x = 0;
            }
        }*/
    }
}



static int CompareMaterials(IGameMaterial *k1, IGameMaterial *k2)
{
    TCHAR *a = k1->GetMaterialName();
    TCHAR *b = k2->GetMaterialName();
    return(_tcscmp(a,b));
}

void Unreal3DExport::SortMaterials()
{
    //Materials.Sort((CompareFnc)CompareMaterials);
}

void Unreal3DExport::Init()
{
    // Init
    CheckCancel();
    pScene = GetIGameInterface();
    GetConversionManager()->SetUserCoordSystem(UnrealCoords);
    if( bExportSelected )
    {
        Tab<INode*> selnodes;;
        for( int i=0; i<pInt->GetSelNodeCount(); ++i )
        {
            INode* n = pInt->GetSelNode(i);
            selnodes.Append(1,&n);
        }
        if( !pScene->InitialiseIGame(selnodes,false)  )
            throw MAXException(GetString(IDS_ERR_IGAME));
    }
    else
    {
        if( !pScene->InitialiseIGame() )
            throw MAXException(GetString(IDS_ERR_IGAME));
    }


    // Enumerate scene
    NodeCount = pScene->GetTotalNodeCount();
    for( int i=0; i<pScene->GetTopLevelNodeCount(); ++i )
    {
        IGameNode * n = pScene->GetTopLevelNode(i);
        ExportNode(n);
    }
    Progress += U3D_PROGRESS_ENUM;


    // Get animation info
    FrameStart = pScene->GetSceneStartTime() / pScene->GetSceneTicks();
    FrameEnd = pScene->GetSceneEndTime() / pScene->GetSceneTicks();
    FrameCount = FrameEnd - FrameStart+1;
    if( FrameCount <= 0 || FrameEnd < FrameStart ) 
    {
        ProgressMsg.printf(GetString(IDS_ERR_FRAMERANGE),FrameStart,FrameEnd);
        throw MAXException(ProgressMsg.data());
    }
    pScene->SetStaticFrame(FrameStart);
}

void Unreal3DExport::GetTris()
{
    
    // Export triangle data
    FJSMeshTri nulltri = FJSMeshTri();
    for( int n=0; n<Nodes.Count(); ++n )
    {
        CheckCancel();
        
        IGameNode* node = Nodes[n];
        IGameMesh* mesh = static_cast<IGameMesh*>(node->GetIGameObject());
        if( mesh->InitializeData() )
        {
            int vertcount = mesh->GetNumberOfVerts();
            int tricount = mesh->GetNumberOfFaces();
            if( vertcount > 0 && tricount > 0 )
            {
                // Progress
                ProgressMsg.printf(GetString(IDS_INFO_MESH),n+1,Nodes.Count(),TSTR(node->GetName()));
                pInt->ProgressUpdate(Progress+(static_cast<float>(n)/Nodes.Count()*U3D_PROGRESS_MESH), FALSE, ProgressMsg.data());

                // Alloc triangles space
                Tris.Resize(Tris.Count()+tricount);

                // Append triangles
                for( int i=0; i!=tricount; ++i )
                {
                    FaceEx* f = mesh->GetFace(i);
                    if( f )
                    {
                        FJSMeshTri tri(nulltri);

                        // TODO: add material id listing
                        RegisterMaterial( node, mesh, f, &tri );

                        tri.iVertex[0] = VertsPerFrame + f->vert[0];
                        tri.iVertex[1] = VertsPerFrame + f->vert[1];
                        tri.iVertex[2] = VertsPerFrame + f->vert[2];

                        Point2 p;
                        if( mesh->GetTexVertex(f->texCoord[0],p) ){
                            tri.Tex[0] = FMeshUV(p);
                        }
                        if( mesh->GetTexVertex(f->texCoord[1],p) ){
                            tri.Tex[1] = FMeshUV(p);
                        }
                        if( mesh->GetTexVertex(f->texCoord[2],p) ){
                            tri.Tex[2] = FMeshUV(p);
                        }
                        
                        Tris.Append(1,&tri);                       
                    }
                }

                VertsPerFrame += vertcount;
            }
            else
            {
                // remove invalid node
                Nodes.Delete(n--,1);
            }
        }
        node->ReleaseIGameObject();
    }
    Progress += U3D_PROGRESS_MESH;
}

void Unreal3DExport::GetAnim()
{
    
    // Export vertex animation
    int lastvert = 0;
    FMeshVert nullvert = FMeshVert();
    Points.SetCount(VertsPerFrame*FrameCount,TRUE);
    for( int t=0; t<FrameCount; ++t )
    {            
        // Progress
        CheckCancel();
        ProgressMsg.printf(GetString(IDS_INFO_ANIM),t+1,FrameCount);
        pInt->ProgressUpdate(Progress+((float)t/FrameCount*U3D_PROGRESS_ANIM), FALSE, ProgressMsg.data());
        
        // Set frame
        int frameverts = 0;
        int curframe = FrameStart + t;
        pScene->SetStaticFrame(curframe);
        
        // Write mesh verts
        for( int n=0; n<Nodes.Count(); ++n )
        {
            CheckCancel();

            IGameMesh * mesh = (IGameMesh*)Nodes[n]->GetIGameObject();          
            if( mesh->InitializeData() )
            {
                int vertcount = mesh->GetNumberOfVerts();
                for( int i=0; i<vertcount; ++i )
                {
                    Point3 p;
                    if( mesh->GetVertex(i,p) )
                    {
                        Points[lastvert++] = p;
                    }
                }
                frameverts += vertcount;
            }
            Nodes[n]->ReleaseIGameObject();
        }

        // Check number of verts in this frame
        if( frameverts != VertsPerFrame )
        {
            ProgressMsg.printf(GetString(IDS_ERR_NOVERTS),curframe,frameverts,VertsPerFrame);
            throw MAXException(ProgressMsg.data());
        }
    }
    Progress += U3D_PROGRESS_ANIM;

}

void Unreal3DExport::WriteTracking()
{
    Tab<Point3> Loc;
    Tab<Quat> Quat;
    Tab<Point3> Euler;

    Loc.SetCount(FrameCount);
    Quat.SetCount(FrameCount);
    Euler.SetCount(FrameCount);

    for( int n=0; n<TrackedNodes.Count(); ++n )
    {
        IGameNode* node = TrackedNodes[n];

        for( int t=0; t<FrameCount; ++t )
        {            
            // Progress
            CheckCancel();
            
            // Set frame
            int curframe = FrameStart + t;
            pScene->SetStaticFrame(curframe);

            // Write tracking
            GMatrix objTM = node->GetWorldTM();
            Loc[t] = objTM.Translation();
            Quat[t] = objTM.Rotation();

            float eu[3];
            QuatToEuler(Quat[t],eu);
            Euler[t]=Point3(eu[0],eu[1],eu[2]);
            Euler[t] *= 180.0f/pi;

            eu[1] *= -1;
            EulerToQuat(eu,Quat[t],EULERTYPE_YXZ);
        }
        
        for( int t=0; t<FrameCount; ++t )
        {    
            _ftprintf( fLog, _T("%sLoc[%d]=(X=%f,Y=%f,Z=%f)\n"), node->GetName(), t, Loc[t].x, Loc[t].y, Loc[t].z );
        }
        
        for( int t=0; t<FrameCount; ++t )
        {    
            _ftprintf( fLog, _T("%sQuat[%d]=(W=%f,X=%f,Y=%f,Z=%f)\n"), node->GetName(), t, Quat[t].w, Quat[t].x, Quat[t].y, Quat[t].z ); 
        }
        
        for( int t=0; t<FrameCount; ++t )
        {    
            _ftprintf( fLog, _T("%sEuler[%d]=(X=%f,Y=%f,Z=%f)\n"), node->GetName(), t, Euler[t].x, Euler[t].y, Euler[t].z ); 
        }
    }
}

/*
    Nodes[n]->GetIGameObject
    _ftprintf( fLog, _T("%sLocation[%d]=(X=%f,Y=%f,Z=%f)\n"), Nodes[n]->GetName(), curframe ); 
*/

void Unreal3DExport::Prepare()
{
    
    // Optimize
    if( bMaxResolution && Points.Count() > 1 )
    {
        pInt->ProgressUpdate(Progress, FALSE, GetString(IDS_INFO_OPT_SCAN));

        Point3 MaxPoint = Points[0];
        Point3 MinPoint = MaxPoint;

        // get scene bounding box
        for( int i=1; i<Points.Count(); ++i )
        {
            if      ( Points[i].x > MaxPoint.x )    MaxPoint.x = Points[i].x;
            else if ( Points[i].x < MinPoint.x )    MinPoint.x = Points[i].x;

            if      ( Points[i].y > MaxPoint.y )    MaxPoint.y = Points[i].y;
            else if ( Points[i].y < MinPoint.y )    MinPoint.y = Points[i].y;

            if      ( Points[i].z > MaxPoint.z )    MaxPoint.z = Points[i].z;
            else if ( Points[i].z < MinPoint.z )    MinPoint.z = Points[i].z;
        }
    
        // get center point
        OptOffset = MaxPoint+MinPoint;
        OptOffset *= 0.5;

        // center bounding box 
        MaxPoint -= OptOffset;
        MinPoint -= OptOffset;  

        // See FMeshVert
        OptScale.x = 1023.0f / max(fabs(MaxPoint.x),fabs(MinPoint.x));
        OptScale.y = 1023.0f / max(fabs(MaxPoint.y),fabs(MinPoint.y));
        OptScale.z = 511.0f / max(fabs(MaxPoint.z),fabs(MinPoint.z));

        // apply adjustments
        pInt->ProgressUpdate(Progress, FALSE, GetString(IDS_INFO_OPT_APPLY));
        for( int i=0; i<Points.Count(); ++i )
        {
            Point3& p = Points[i];
            p -= OptOffset;
            p *= OptScale;   
        }
    }
    
    // Convert verts
    Verts.SetCount(Points.Count(),TRUE);
    for( int i=0; i<Points.Count(); ++i )
    {
        Verts[i] = FMeshVert(Points[i]);
    }
}

void Unreal3DExport::WriteScript()
{
    // Write script file
    {

        fScript = _tfopen(ScriptFileName,_T("wb"));
        if( !fScript )
        {
            ProgressMsg.printf(GetString(IDS_ERR_FSCRIPT),ScriptFileName);
            throw MAXException(ProgressMsg.data());
        }

        TSTR buf;


        // Write class def     
        _ftprintf( fScript, _T("class %s extends Object;\n\n"), FileName ); 

        // write import
        _ftprintf( fScript, _T("#exec MESH IMPORT MESH=%s ANIVFILE=%s_a.3D DATAFILE=%s_d.3D \n"), FileName, FileName, FileName ); 
        
        // write origin & rotation
        // TODO: figure out why it's incorrect without -1
        Point3 porg = OptOffset * OptScale * -1; 
        _ftprintf( fScript, _T("#exec MESH ORIGIN MESH=%s X=%f Y=%f Z=%f PITCH=%d YAW=%d ROLL=%d \n"), FileName, porg.x, porg.y, porg.z, (int)OptRot.x, (int)OptRot.y, (int)OptRot.z ); 
        
        // write mesh scale
        Point3 psc( Point3(1.0f/OptScale.x,1.0f/OptScale.y,1.0f/OptScale.z));
        _ftprintf( fScript, _T("#exec MESH SCALE MESH=%s X=%f Y=%f Z=%f \n"), FileName, psc.x, psc.y, psc.z ); 
        
        // write meshmap
        _ftprintf( fScript, _T("#exec MESHMAP NEW MESHMA=P%s MESH=%smap \n"), FileName, FileName ); 
        
        // write meshmap scale
        _ftprintf( fScript, _T("#exec MESHMAP SCALE MESHMAP=%s X=%f Y=%f Z=%f \n"), FileName, psc.x, psc.y, psc.z ); 

        // write sequence
        _ftprintf( fScript, _T("#exec MESH SEQUENCE MESH=%s SEQ=%s STARTFRAME=%d NUMFRAMES=%d \n"), FileName, _T("All"), 0, FrameCount-1 ); 

        // Get World NoteTrack
        ReferenceTarget *rtscene = pInt->GetScenePointer();
        for( int t=0; t<rtscene->NumNoteTracks(); ++t )
        {
            DefNoteTrack* notetrack = static_cast<DefNoteTrack*>(rtscene->GetNoteTrack(t));
            for( int k=0; k<notetrack->keys.Count(); ++k )
            {


                NoteKey* notekey = notetrack->keys[k];                        
                TSTR text = notekey->note;
                int notetime = notekey->time / pScene->GetSceneTicks();

                while( !text.isNull() )
                {
                    TSTR cmd = SplitStr(text,_T('\n'));
                    
                    if( MatchPattern(cmd,TSTR(_T("a *")),TRUE) )
                    {
                        SplitStr(cmd,_T(' '));
                        TSTR seq = SplitStr(cmd,_T(' '));
                        int end = _ttoi(SplitStr(cmd,_T(' ')));;
                        TSTR rate = SplitStr(cmd,_T(' '));
                        TSTR group = SplitStr(cmd,_T(' '));

                        if( seq.isNull() )
                        {
                            ProgressMsg.printf(_T("Missing animation name in notekey #%d"),notetime);
                            throw MAXException(ProgressMsg.data());
                        }
                        
                        if( end <= notetime )
                        {
                            ProgressMsg.printf(_T("Invalid animation endframe (%d) in notekey #%d"),end,notetime);
                            throw MAXException(ProgressMsg.data());
                        }

                        int startframe = notetime;
                        int numframes = end - notetime;

                        buf.printf( _T("#exec MESH SEQUENCE MESH=%s SEQ=%s STARTFRAME=%d NUMFRAMES=%d"), FileName, seq, notetime, numframes );

                        if( !rate.isNull() )
                            buf.printf( _T("%s RATE=%f"), buf, rate );
                        
                        if( !group.isNull() )
                            buf.printf( _T("%s GROUP=%f"), buf, rate );
                        
                        SeqName = seq;
                        SeqFrame = startframe;

                        buf.printf( _T("%s \n"), buf );
                        _fputts( buf, fScript ); 

                    }
                    else if( MatchPattern(cmd,TSTR(_T("n *")),TRUE) )
                    {
                        SplitStr(cmd,_T(' '));
                        TSTR func = SplitStr(cmd,_T(' '));
                        TSTR time = SplitStr(cmd,_T(' '));
                        
                        if( func.isNull() )
                        {
                            ProgressMsg.printf(_T("Missing notify name in notekey #%d"),notetime);
                            throw MAXException(ProgressMsg.data());
                        }

                        if(  time.isNull() )
                        {
                            ProgressMsg.printf(_T("Missing notify time in notekey #%d"),notetime);
                            throw MAXException(ProgressMsg.data());
                        }

                        buf.printf( _T("#exec MESH NOTIFY MESH=%s SEQ=%s TIME=%s FUNCTION=%s"), FileName, SeqName, time, func );
                        
                        _fputts( buf, fScript ); 
                    }




                }
            }
        }


        // TODO: write materials
        if( Materials.Count() > 0 )
        {
            for( int i=0; i<Materials.Count(); ++i )
            {
                IGameMaterial* mat = Materials[i].Mat;
                if( mat == NULL )
                    continue;

                _ftprintf( fScript, _T("#exec MESHMAP SETTEXTURE MESHMAP=%s NUM=%d TEXTURE=%s \n"), FileName, i, _T("DefaultTexture") ); 
            }
        }

    }
}

void Unreal3DExport::WriteModel()
{
    // Progress
        pInt->ProgressUpdate(Progress, FALSE, GetString(IDS_INFO_WRITE));

        // Open data file
        fMesh = _tfopen(ModelFileName,_T("wb"));
        if( !fMesh ) 
        {
            ProgressMsg.printf(GetString(IDS_ERR_FMODEL),ModelFileName);
            throw MAXException(ProgressMsg.data());
        }

        // Open anim file
        fAnim = _tfopen(AnimFileName,_T("wb"));
        if( !fAnim )
        {
            ProgressMsg.printf(GetString(IDS_ERR_FANIM),AnimFileName);
            throw MAXException(ProgressMsg.data());
        }
        
        // data headers
        hData.NumPolys = Tris.Count();
        hData.NumVertices = VertsPerFrame;

        // anim headers
        hAnim.FrameSize = VertsPerFrame * sizeof(FMeshVert); 
        hAnim.NumFrames = FrameCount;


        // Progress
        CheckCancel();
        pInt->ProgressUpdate(Progress, FALSE, GetString(IDS_INFO_WMESH));
        

        // Write data
        fwrite(&hData,sizeof(FJSDataHeader),1,fMesh);
        if( Tris.Count() > 0 )
        {
            fwrite(Tris.Addr(0),sizeof(FJSMeshTri),Tris.Count(),fMesh);
        }
        Progress += U3D_PROGRESS_WMESH;

        // Progress
        CheckCancel();
        pInt->ProgressUpdate(Progress, FALSE, GetString(IDS_INFO_WANIM));

        // Write anim
        fwrite(&hAnim,sizeof(FJSAnivHeader),1,fAnim);
        if( Verts.Count() > 0 )
        {
            fwrite(Verts.Addr(0),sizeof(FMeshVert),Verts.Count(),fAnim);
        }
        Progress += U3D_PROGRESS_WANIM;
}

void Unreal3DExport::ShowSummary()
{
    
    // Progress
    pInt->ProgressUpdate(Progress);

    // Display Summary
    if( bShowPrompts )
    {
        ProgressMsg.printf(GetString(IDS_INFO_SUMMARY)
            , hAnim.NumFrames
            , hData.NumPolys
            , hData.NumVertices);

        if( bMaxResolution )
        {
            TSTR buf;
            buf.printf(GetString(IDS_INFO_DIDPRECISION)
                , FileName);
            ProgressMsg += buf;
        }

        MaxMsgBox(pInt->GetMAXHWnd(),ProgressMsg.data(),ShortDesc(),MB_OK|MB_ICONINFORMATION);
    }
}


TSTR Unreal3DExport::GetCfgFileName()
{
    TSTR FileName;
    
    FileName += GetCOREInterface()->GetDir(APP_PLUGCFG_DIR);
    FileName += "\\";
    FileName += "Unreal3DExport.cfg";
    return FileName;
}


BOOL Unreal3DExport::ReadConfig()
{
    //TSTR FileName = GetCfgFileName();
    //FILE* cfgStream;

    //cfgStream = fopen(FileName, "rb");
    //if (!cfgStream)
    //    return FALSE;
    //
    //fclose(cfgStream);
    return TRUE;
}

void Unreal3DExport::WriteConfig()
{
    //TSTR FileName = GetCfgFileName();
    //FILE* cfgStream;

    //cfgStream = fopen(FileName, "wb");
    //if (!cfgStream)
    //    return;

    //
    //fclose(cfgStream);
}



int Unreal3DExport::ExtCount()
{
    //TODO: Returns the number of file name extensions supported by the plug-in.
    return 1;
}

const TCHAR *Unreal3DExport::Ext(int n)
{       
    //TODO: Return the 'i-th' file name extension (i.e. "3DS").
    return _T("3d");
}

const TCHAR *Unreal3DExport::LongDesc()
{
    //TODO: Return long ASCII description (i.e. "Targa 2.0 Image File")
    return _T("Unreal 3D Vertex Animation");
}
    
const TCHAR *Unreal3DExport::ShortDesc() 
{           
    //TODO: Return short ASCII description (i.e. "Targa")
    return _T("Unreal 3D");
}

const TCHAR *Unreal3DExport::AuthorName()
{           
    //TODO: Return ASCII Author name
    return _T("Roman Switch` Dzieciol");
}

const TCHAR *Unreal3DExport::CopyrightMessage() 
{   
    // Return ASCII Copyright message
    return _T("Copyright (C) 2007");
}

const TCHAR *Unreal3DExport::OtherMessage1() 
{       
    //TODO: Return Other message #1 if any
    return _T("");
}

const TCHAR *Unreal3DExport::OtherMessage2() 
{       
    //TODO: Return other message #2 in any
    return _T("");
}

unsigned int Unreal3DExport::Version()
{               
    //TODO: Return Version number * 100 (i.e. v3.01 = 301)
    return 100;
}

void Unreal3DExport::ShowAbout(HWND hWnd)
{           
    // Optional
}

BOOL Unreal3DExport::SupportsOptions(int ext, DWORD options)
{
    // TODO Decide which options to support.  Simply return
    // true for each option supported by each Extension 
    // the exporter supports.

    return TRUE;
}

