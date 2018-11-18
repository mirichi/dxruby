void DXRuby_raise( int errorcode, char *msg );
void InitMessageThread( void );
void ExitMessageThread( void );
void WindowCreateMessage( void );
int ResetMessage( void );
void ShowCursorMessage( void );
void HideCursorMessage( void );
void SetCursorMessage( int cursorname );
void SetImeEnable( int flag );
int GetImeEnable( void );

enum DXRubyErrorMessage
{
    ERR_OUTOFMEMORY = 1,
    ERR_INTERNAL,
    ERR_NOEXISTSCREENMODE,
    ERR_WINDOWCREATE,
    ERR_D3DERROR,
    ERR_DINPUTERROR,
    ERR_MAX
};

