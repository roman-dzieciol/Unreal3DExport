

class MyErrorProc : public IGameErrorCallBack
{
public:
    void ErrorProc(IGameError error)
    {
        TCHAR * buf = GetLastIGameErrorText();
        DebugPrint(_T("ErrorCode = %d ErrorText = %s\n"), error,buf);
    }
};


// Dummy function for progress bar
DWORD WINAPI fn(LPVOID arg)
{
    return 0;
}

void fclosen( FILE*& f )
{
    if( f != NULL )
    {
        fclose(f);
        f = NULL;
    }
}


#define U3D_PROGRESS_INIT   5.0f
#define U3D_PROGRESS_ENUM   10.0f
#define U3D_PROGRESS_MESH   25.0f
#define U3D_PROGRESS_ANIM   50.0f
#define U3D_PROGRESS_WMESH  5.0f
#define U3D_PROGRESS_WANIM  5.0f


class CancelException : public MAXException
{
public:
    CancelException(TCHAR* msg=NULL, int code=0) : MAXException(msg,code)
    {
    }
};


/*UserCoord UnrealCoords = {
    0,  //Left Handed
    4,  //X axis goes in
    1,  //Y Axis goes right
    2,  //Z Axis goes up.
    0,  //U Tex axis is right
    0,  //V Tex axis is Down
};  */

UserCoord UnrealCoords = {
    1, 
    1, 
    4, 
    2, 
    0, 
    0, 
};  

/*
BOOL CALLBACK Unreal3DExportOptionsDlgProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam) 
{
	static Unreal3DExport *exp = NULL; 

    switch(message) {
        case WM_INITDIALOG:
			exp = (Unreal3DExport *)lParam;
			//SetWindowLongPtr(hWnd,GWLP_USERDATA,lParam); 
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

        case WM_CLOSE:
            EndDialog(hWnd, 0);
            return TRUE;
    }
    return FALSE;
}*/

Tab<TSTR*> TokStr( TSTR text, TSTR sep )
{
    Tab<TSTR*> tokens;

    int last=0;
    for( int i=0; i!=text.length(); ++i )
    {
        for( int s=0; s!=sep.length(); ++s )
        {
            if( text[i] == sep[s] )
            {
                TSTR* buf = new TSTR(text.Substr(last,i-last));
                tokens.Append(1,&buf);
                last = i+1;
            }
        }
    }

    if( i > last )
    {
        TSTR* buf = new TSTR(text.Substr(last,i-last));
        tokens.Append(1,&buf);
    }

    return tokens;
}


TSTR SplitStr( TSTR& text, TCHAR sep )
{
    int s = text.first(sep);
    if( s != -1 )
    {
        TSTR left = text.Substr(0,s++);
        text = text.Substr(s,text.Length()-s);
        return left;
    }
    else
    {
        TSTR left = text;
        text.Resize(0);
        return left;
    }
}

TSTR StrRepl( const TSTR& text, TCHAR from, TCHAR to )
{
    TSTR buf = text;
    for( int s=buf.first(from); s!=-1; s=buf.first(from) ) 
        buf[s]=_T(to);
    return buf;
}

