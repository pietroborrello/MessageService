// Client.cpp : definisce il punto di ingresso dell'applicazione console.
//

#include "Client.h"
#include "aes.h"
#include "linked_list.h"
#include "rsa.h"
#include "stdafx.h"

SOCKET server_desc = NULL;   /*  connection socket         */
struct sockaddr_in servaddr; /*  socket address structure  */

UINT command_send_type = 0;
UINT term_type = 0;
UINT msg_type = 0;
_TCHAR username[FIELD_LEN] = {0};
_TCHAR psw[FIELD_LEN] = {0};
bool loggedIn = FALSE;

HWND hWindow, hWndListView, hEditMsg, hEditTo, hEditObj, hSendButton,
    hGetButton, hNewButton, hShowButton, hBackButton, hLabelTo, hLabelObj,
    hLoginButton, hRegisterCheckBox, hRemoveButton, hSyncCheck;

FILE *userMsgs = NULL;
linked_list *msgs = NULL;

// colors
COLORREF background = RGB(250, 253, 255);
static HBRUSH hBrush = CreateSolidBrush(background);
static HFONT hfont = CreateFont(
    17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
    CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_MODERN, __T("Helvetica"));

bool CALLBACK SetFont(HWND child) {
  SendMessage(child, WM_SETFONT, (LPARAM)hfont, true);
  return true;
}

HWND CreateListView(HWND hWndParent) {
  HWND hWndList; // Handle to the list view window
  RECT rcl;      // Rectangle for setting the size of the window
  MSGINFO *msgInfo;
  int index;     // Index used in for loops
  LV_COLUMN lvC; // List View Column structure
  int iSubItem;  // Index for inserting sub items
  LV_ITEM lvI;   // List view item structure
  int ret;

  // Ensure that the common control DLL is loaded.
  InitCommonControls();
  // Get the size and position of the parent window
  GetClientRect(hWndParent, &rcl);

  // Create the list view window that starts out in report view
  // and allows label editing.
  hWndList = CreateWindowEx(WS_EX_CLIENTEDGE,
                            WC_LISTVIEW, // list view class
                            TEXT(""),    // no default text
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT |
                                LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                            150, 10, rcl.right - rcl.left - 190, 500,
                            hWndParent, (HMENU)ID_LIST, NULL, NULL);
  handle_error(hWndList == NULL, "Unable to create list view.\n");

  ListView_SetExtendedListViewStyle(hWndList, LVS_EX_FULLROWSELECT);

  // initialize the list view window
  // initialize the columns we will need
  // Initialize the LV_COLUMN structure
  // the mask specifies that the .fmt, .ex, width, and .subitem members
  // of the structure are valid,
  lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
  lvC.fmt = LVCFMT_LEFT; // left align the column
  lvC.cx = 150;          // width of the column, in pixels
  lvC.pszText = NULL;    // szText;

  // Add Columns
  lvC.iSubItem = 0;
  lvC.pszText = TEXT("From");
  if (ListView_InsertColumn(hWndList, lvC.iSubItem, &lvC) == -1)
    return NULL;

  lvC.iSubItem = 1;
  lvC.pszText = TEXT("Obj");
  if (ListView_InsertColumn(hWndList, lvC.iSubItem, &lvC) == -1)
    return NULL;

  lvC.iSubItem = 2;
  lvC.pszText = TEXT("Msg");
  lvC.cx = 729;
  if (ListView_InsertColumn(hWndList, lvC.iSubItem, &lvC) == -1)
    return NULL;

  // Finally, let's add the actual items to the control
  // Fill in the LV_ITEM structure for each of the items to add
  // to the list.
  // The mask specifies the the .pszText, .iImage, .lParam and .state
  // members of the LV_ITEM structure are valid.
  lvI.mask = LVIF_TEXT | LVIF_PARAM;

  for (index = 0; index < linked_list_size(msgs); index++) {
    lvI.iItem = index;
    lvI.iSubItem = 0;
    // The parent window is responsible for storing the text. The List view
    // window will send a LVN_GETDISPINFO when it needs the text to display/
    lvI.pszText = LPSTR_TEXTCALLBACK;
    lvI.cchTextMax = MSG_LEN;
    ret = linked_list_get(msgs, index, (any_t *)&msgInfo);
    handle_error(ret == LINKED_LIST_NOK, "Unable to retrieve message.\n");
    lvI.lParam = (LPARAM)msgInfo;
    if (ListView_InsertItem(hWndList, &lvI) == -1)
      return NULL;
    for (iSubItem = 1; iSubItem < 2; iSubItem++) {
      ListView_SetItemText(hWndList, index, iSubItem, LPSTR_TEXTCALLBACK);
    }
  }
  return (hWndList);
}

LRESULT NotifyHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  LV_DISPINFO *pLvdi = (LV_DISPINFO *)lParam;
  NM_LISTVIEW *pNm = (NM_LISTVIEW *)lParam;
  MSGINFO *pMsg = (MSGINFO *)(pLvdi->item.lParam);
  static TCHAR szText[10];
  if (wParam != ID_LIST)
    return 0L;
  switch (pLvdi->hdr.code) {
  case LVN_GETDISPINFO:
    switch (pLvdi->item.iSubItem) {
    case 0: // From
      pLvdi->item.pszText = pMsg->from;
      break;
    case 1: // Obj
      pLvdi->item.pszText = pMsg->obj;
      break;
    case 2: // Msg
      pLvdi->item.pszText = pMsg->msg;
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  return 0L;
}

void InitInterface(HWND hWindow) {
  HWND ret;
  SetFont(hWindow);
  ret = hNewButton =
      CreateWindow(TEXT("BUTTON"), TEXT("NEW"), WS_CHILD | BS_PUSHBUTTON, 10,
                   10, 100, 50, hWindow, (HMENU)NEW_CMD, NULL, NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hNewButton);
  ret = hLoginButton = CreateWindow(
      TEXT("BUTTON"), TEXT("LOGIN"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10,
      10, 100, 50, hWindow, (HMENU)LOGIN_CMD, NULL, NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hLoginButton);

  ret = hRegisterCheckBox =
      CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("Register and Login"),
                     BS_CHECKBOX | WS_VISIBLE | WS_CHILD, 200, 90, 300, 30,
                     hWindow, (HMENU)REGISTER_CMD, NULL, NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hRegisterCheckBox);

  ret = hSyncCheck = CreateWindowEx(
      NULL, TEXT("BUTTON"),
      TEXT("Request Synchronization (Use Moderately: leave disabled if you use "
           "your account on a single device)"),
      BS_CHECKBOX | WS_CHILD, 10, 650, 900, 30, hWindow, (HMENU)SYNC_CMD, NULL,
      NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hSyncCheck);

  ret = hShowButton =
      CreateWindow(TEXT("BUTTON"), TEXT("SHOW"), WS_CHILD | BS_PUSHBUTTON, 10,
                   70, 100, 50, hWindow, (HMENU)SHOW_CMD, NULL, NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hShowButton);
  ret = hRemoveButton =
      CreateWindow(TEXT("BUTTON"), TEXT("REMOVE"), WS_CHILD | BS_PUSHBUTTON, 10,
                   130, 100, 50, hWindow, (HMENU)RM_CMD, NULL, NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hRemoveButton);
  ret = hBackButton =
      CreateWindow(TEXT("BUTTON"), TEXT("BACK"), WS_CHILD | BS_PUSHBUTTON, 10,
                   10, 100, 50, hWindow, (HMENU)BACK_CMD, NULL, NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hBackButton);
  ret = hSendButton =
      CreateWindow(TEXT("BUTTON"), TEXT("SEND"), WS_CHILD | BS_PUSHBUTTON, 10,
                   70, 100, 50, hWindow, (HMENU)SEND_CMD, NULL, NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hSendButton);
  ret = hGetButton = CreateWindow(TEXT("BUTTON"), TEXT("RECEIVED..."),
                                  WS_CHILD | BS_PUSHBUTTON, 10, 10, 100, 50,
                                  hWindow, (HMENU)RECV_CMD, NULL, NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hGetButton);
  ret = hEditTo = CreateWindowEx(
      WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_TABSTOP, 200,
      10, 300, 25, hWindow, NULL, GetModuleHandle(NULL), NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hEditTo);
  ret = hLabelTo = CreateWindowEx(
      NULL, TEXT("STATIC"), TEXT("User:"), WS_CHILD | WS_VISIBLE | SS_LEFT, 150,
      15, 40, 25, hWindow, NULL, GetModuleHandle(NULL), NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hLabelTo);
  ret = hEditObj = CreateWindowEx(
      WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_TABSTOP, 200,
      55, 300, 25, hWindow, NULL, GetModuleHandle(NULL), NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hEditObj);
  ret = hLabelObj = CreateWindowEx(
      NULL, TEXT("STATIC"), TEXT("Psw:"), WS_CHILD | WS_VISIBLE | SS_LEFT, 150,
      60, 40, 25, hWindow, NULL, GetModuleHandle(NULL), NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hLabelObj);
  ret = hEditMsg = CreateWindowEx(
      WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
      WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, 200,
      100, 1000, 500, hWindow, (HMENU)SEND_EDIT, GetModuleHandle(NULL), NULL);
  handle_error(ret == NULL, "Unable to init GUI.\n");
  SetFont(hEditMsg);
  return;
}

void OpenMsgsFile() {
  int ret;
  char isOk[FIELD_LEN];
  char from[FIELD_LEN], to[FIELD_LEN], obj[2 * FIELD_LEN], msg[MSG_LEN];
  char format[36]; //"%[^|]|%[^|]|%[^|]|%[^|]|%[^}]}\r\n"
  sprintf_s(format, 36, "%%[^%c]%c%%[^%c]%c%%[^%c]%c%%[^%c]%c%%[^%c]%c\r\n",
            RECORD_SEP, RECORD_SEP, RECORD_SEP, RECORD_SEP, RECORD_SEP,
            RECORD_SEP, RECORD_SEP, RECORD_SEP, MSG_SEP, MSG_SEP);
  if (!CreateDirectory("msgs", NULL) &&
      ERROR_ALREADY_EXISTS != GetLastError()) {
    MessageBox(NULL, TEXT("Unable to open msgs directory\n"), NULL,
               MB_OK | MB_ICONERROR);
  };
  msgs = linked_list_new();
  handle_error(msgs == NULL, "No space to store msgs.\n");
  char filename[FIELD_LEN + 4];
  sprintf_s(filename, FIELD_LEN + 4, "msgs/%s.txt", username);
  HANDLE h = CreateFile(filename, GENERIC_WRITE | GENERIC_READ, 0, NULL,
                        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  handle_error(h == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS,
               TEXT("Unable to create msgs file.\n"));
  CloseHandle(h);
  ret = fopen_s(&userMsgs, filename, "r+");
  handle_error(ret != 0, "Unable to open msgs file.\n");
  while ((ret = fscanf_s(userMsgs, format, isOk, FIELD_LEN - 1, from,
                         FIELD_LEN - 1, to, FIELD_LEN - 1, obj,
                         2 * FIELD_LEN - 1, msg, MSG_LEN - 1)) != EOF) {
    if (ret != 5)
      break;
    if (isOk[0] == '+') {
      MSGINFO *msgInfo = (MSGINFO *)malloc(sizeof(MSGINFO));
      handle_error(msgInfo == NULL, "No space to store msgs.\n");

      _stprintf_s(msgInfo->from, FIELD_LEN, TEXT("%s"), from);
      _stprintf_s(msgInfo->obj, 2 * FIELD_LEN, TEXT("%s"), obj);
      _stprintf_s(msgInfo->msg, MSG_LEN, TEXT("%s"), msg);
      ret = linked_list_add(msgs, msgInfo);
      handle_error(ret == LINKED_LIST_NOK, "No space to store msgs.\n");
    }
  }
  return;
}

void UpdateReceivedMsgs() {
  int ret, recv_bytes, sent_bytes, msg_len;
  char num;
  _TCHAR buf[MSG_LEN];
  _TCHAR enc_buf[MSG_LEN];
  // send get}
  sent_bytes = 0;
  bool sync = IsDlgButtonChecked(hWindow, SYNC_CMD);
  CheckDlgButton(hWindow, SYNC_CMD, BST_UNCHECKED);
  if (sync) {
    _stprintf_s(buf, MSG_LEN, TEXT("%s%c"), SYNC, MSG_SEP);
    // remove saved messages
    while (linked_list_remove(msgs, 0) == LINKED_LIST_OK)
      ;
    fseek(userMsgs, 0, SEEK_SET);
  } else
    _stprintf_s(buf, MSG_LEN, TEXT("%s%c"), GET_CMD, MSG_SEP);
  msg_len = _tcslen(buf) * sizeof(buf[0]);
  ret = 0;
  num = 0;
  while (ret < 1) // send command byte 0
  {
    ret = send(server_desc, &num, 1, 0);
    if (ret < 0 && WSAGetLastError() == WSAEINTR)
      continue;
    handle_error(ret == 0, "Socket closed\n.");
    handle_error(ret < 0, "Cannot write to the socket.\n");
  }
  while (sent_bytes < msg_len) // send command
  {
    ret = send(server_desc, (char *)buf + sent_bytes, msg_len - sent_bytes, 0);
    if (ret < 0 && WSAGetLastError() == WSAEINTR)
      continue;
    handle_error(ret == 0, "Socket closed\n.");
    handle_error(ret < 0, "Cannot write to the socket");
    sent_bytes += ret;
  }
  // get num
  ret = 0;
  while (ret < 1) {
    ret = recv(server_desc, &num, 1, 0);
    if (ret < 0 && WSAGetLastError() == WSAEINTR)
      continue;
    handle_error(ret == 0, "Socket closed\n.");
    handle_error(ret < 0, "Cannot read from the socket.\n");
  }
  // update msgs
  while (num > 0) {
    memset(enc_buf, 0, MSG_LEN);
    memset(buf, 0, MSG_LEN);
    ret = 0;
    unsigned char n;
    while (ret < 1) {
      ret = recv(server_desc, (char *)&n, 1, 0);
      if (ret < 0 && WSAGetLastError() == WSAEINTR)
        continue;
      handle_error(ret < 0, "Cannot recv from the socket");
    }
    int to_receive = n * KEY_LEN;
    num--;
    recv_bytes = 0;
    while (recv_bytes < to_receive && recv_bytes < MSG_LEN - 1) {
      ret = recv(server_desc, (char *)enc_buf + recv_bytes, 1, 0);
      if (ret < 0 && WSAGetLastError() == WSAEINTR)
        continue;
      handle_error(ret == 0, "Socket closed\n.");
      handle_error(ret < 0, "Cannot recv from socket");
      recv_bytes += ret;
    }
    unsigned char key[KEY_LEN] = {0};
    unsigned char iv[KEY_LEN] = {IV_INIT};
    memcpy(key, psw, strlen(psw));
    AES128_CBC_decrypt_buffer((unsigned char *)buf, (unsigned char *)enc_buf,
                              recv_bytes, (unsigned char *)key,
                              (unsigned char *)iv);
    // closing string
    // buf[recv_bytes] = '\0';

    // save msg in file
    if (!sync)
      fseek(userMsgs, 0, SEEK_END);
    fprintf_s(userMsgs, "+%c%s\r\n", RECORD_SEP, buf);
    // load msg in memory
    MSGINFO *msgInfo = (MSGINFO *)malloc(sizeof(MSGINFO));
    _TCHAR trash[FIELD_LEN];
    _TCHAR format[36]; //"%[^|]|%[^|]|%[^|]|%[^}]}"
    sprintf_s(format, 36, "%%[^%c]%c%%[^%c]%c%%[^%c]%c%%[^%c]%c", RECORD_SEP,
              RECORD_SEP, RECORD_SEP, RECORD_SEP, RECORD_SEP, RECORD_SEP,
              MSG_SEP, MSG_SEP);
    _stscanf_s(buf, TEXT(format), msgInfo->from, FIELD_LEN - 1, trash,
               FIELD_LEN - 1, msgInfo->obj, 2 * FIELD_LEN - 1, msgInfo->msg,
               MSG_LEN - 1);
    ret = linked_list_add(msgs, msgInfo);
    handle_error(ret == LINKED_LIST_NOK, "No space to store msgs.\n");
  }
  return;
}

char login(BOOL isRegistering) {
  int ret, sent_bytes;
  char msg_len;
  char isOk = 0;

  /*  Create the socket  */
  server_desc = socket(AF_INET, SOCK_STREAM, 0);
  handle_error(server_desc == INVALID_SOCKET,
               "client: error during the creation of the socket.\n");
  // connect to server
  ret = connect(server_desc, (struct sockaddr *)&servaddr, sizeof(servaddr));
  handle_error(ret == SOCKET_ERROR,
               "client: error during connection to server (offline).\n");

  // login
  _TCHAR login_buf[3 * FIELD_LEN];
  int *enc_login_buf;
  if (!loggedIn) {
    ret = GetWindowTextLength(hEditObj);
    if (ret == 0)
      return 1;
    else if (ret > FIELD_LEN - 1) {
      MessageBox(NULL, TEXT("Psw too long (max 16)\n"), NULL,
                 MB_OK | MB_ICONERROR);
      return 1;
    }
    ret = GetWindowTextLength(hEditTo);
    if (ret == 0)
      return 1;
    else if (ret > FIELD_LEN - 1) {
      MessageBox(NULL, TEXT("Username too long (max 16)\n"), NULL,
                 MB_OK | MB_ICONERROR);
      return 1;
    }
    GetWindowText(hEditTo, username, FIELD_LEN);
    GetWindowText(hEditObj, psw, FIELD_LEN);
  }
  if (!isRegistering)
    _stprintf_s(login_buf, sizeof(login_buf), TEXT("%s %s"), username, psw);
  else
    _stprintf_s(login_buf, sizeof(login_buf), TEXT("%s %s %s"), TEXT(REGISTER),
                username, psw);
  sent_bytes = 0;
  msg_len = _tcslen(login_buf) * sizeof(login_buf[0]) + 1; // +'\0'
  enc_login_buf = encodeMessage(msg_len, 1, login_buf, RSA_PUB, RSA_MOD);
  msg_len *= sizeof(int) / sizeof(char);
  ret = 0;
  while (ret < 1) // send lenght of message
  {
    ret = send(server_desc, &msg_len, 1, 0);
    if (ret < 0 && WSAGetLastError() == WSAEINTR)
      continue;
    handle_error(ret == 0, "Socket closed\n.");
    handle_error(ret < 0, "Cannot write to the socket.\n");
  }
  while (sent_bytes < msg_len) // send message
  {
    ret = send(server_desc, ((char *)enc_login_buf) + sent_bytes,
               msg_len - sent_bytes, 0);
    if (ret < 0 && WSAGetLastError() == WSAEINTR)
      continue;
    handle_error(ret < 0, "Cannot write to the socket.\n");
    sent_bytes += ret;
  }
  ret = 0;
  while (ret < 1) {
    ret = recv(server_desc, &isOk, 1, 0);
    if (ret < 0 && WSAGetLastError() == WSAEINTR)
      continue;
    handle_error(ret == 0, "Socket closed\n.");
    handle_error(ret < 0, "Cannot read from the socket.\n");
  }
  free(enc_login_buf);
  return isOk;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  switch (message) {
  case WM_CREATE: {
    InitInterface(hWnd);
    return 0;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hDC = BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    return 0;
  }
  case WM_CTLCOLORSTATIC: {
    HDC hdcStatic = (HDC)wParam;
    SetTextColor(hdcStatic, RGB(0, 0, 0));
    SetBkColor(hdcStatic, background);
    return (INT_PTR)hBrush;
  }
  case WM_DESTROY:
    if (userMsgs != NULL)
      fclose(userMsgs);
    if (server_desc != NULL)
      closesocket(server_desc);
    if (msgs != NULL)
      linked_list_delete(msgs);
    DeleteObject(hfont);
    PostQuitMessage(WM_QUIT);
    return 1;
  case WM_NOTIFY:
    return (NotifyHandler(hWnd, message, wParam, lParam));

  case WM_COMMAND: {
    switch (LOWORD(wParam)) {
    case SEND_CMD: {
      int ret, sent_bytes, msg_len;
      _TCHAR to[FIELD_LEN];
      _TCHAR obj[2 * FIELD_LEN];
      _TCHAR msg_buf[MSG_LEN];
      ret = GetWindowTextLength(hEditTo);
      if (ret > FIELD_LEN - 1) {
        MessageBox(NULL, TEXT("Receiver: Invalid Length! (max 16)\n"), NULL,
                   MB_OK | MB_ICONERROR);
        return 1;
      }
      SendMessage(hEditTo, WM_GETTEXT, sizeof(to) / sizeof(to[0]), (LPARAM)to);
      ret = GetWindowTextLength(hEditObj);
      if (ret > 2 * FIELD_LEN - 1) {
        MessageBox(NULL, TEXT("Object: Invalid Length! (max 32)\n"), NULL,
                   MB_OK | MB_ICONERROR);
        return 1;
      }
      SendMessage(hEditObj, WM_GETTEXT, sizeof(obj) / sizeof(obj[0]),
                  (LPARAM)obj);
      SendMessage(hEditMsg, WM_GETTEXT, sizeof(msg_buf) / sizeof(msg_buf[0]),
                  (LPARAM)msg_buf);

      _TCHAR buf[MSG_LEN];
      if (_tcslen(to) == 0) {
        MessageBox(NULL, TEXT("Insert Receiver!\n"), NULL,
                   MB_OK | MB_ICONWARNING);
      } else if (_tcslen(obj) == 0) {
        MessageBox(NULL, TEXT("Insert Object!\n"), NULL,
                   MB_OK | MB_ICONWARNING);
      } else if (_tcslen(username) + _tcslen(to) + _tcslen(obj) +
                     _tcslen(msg_buf) + 4 >
                 MSG_LEN - 1) {
        MessageBox(NULL, TEXT("Message too big.(max 1024)\n"), NULL,
                   MB_OK | MB_ICONWARNING);
      } else {
        _stprintf_s(buf, MSG_LEN, TEXT("%s%c%s%c%s%c%s%c"), username,
                    RECORD_SEP, to, RECORD_SEP, obj, RECORD_SEP, msg_buf,
                    MSG_SEP);
        sent_bytes = 0;
        msg_len = _tcslen(buf) * sizeof(buf[0]);
        _TCHAR enc_buf[MSG_LEN];
        _TCHAR key[KEY_LEN] = {0};
        _TCHAR iv[KEY_LEN] = {IV_INIT};
        memcpy(key, psw, strnlen(psw, FIELD_LEN));
        AES128_CBC_encrypt_buffer((unsigned char *)enc_buf,
                                  (unsigned char *)buf, msg_len,
                                  (unsigned char *)key, (unsigned char *)iv);
        msg_len += KEY_LEN - (msg_len % KEY_LEN); // round up
        _TCHAR num = msg_len / KEY_LEN;
        // login
        ret = login(FALSE);
        if (ret == 0) {
          MessageBox(NULL, TEXT("Server offline"), NULL, MB_OK | MB_ICONERROR);
          closesocket(server_desc);
          return 1;
        }
        ret = 0;
        while (ret < 1) // send lenght of message
        {
          ret = send(server_desc, &num, 1, 0);
          if (ret < 0 && WSAGetLastError() == WSAEINTR)
            continue;
          handle_error(ret == 0, "Socket closed\n.");
          handle_error(ret < 0, "Cannot write to the socket.\n");
        }
        while (sent_bytes < msg_len) {
          ret = send(server_desc, (char *)enc_buf + sent_bytes,
                     msg_len - sent_bytes, 0);
          if (ret < 0 && WSAGetLastError() == WSAEINTR)
            continue;
          handle_error(ret < 0, "Cannot write to the socket.\n");
          sent_bytes += ret;
        }
        SendMessage(hEditTo, WM_SETTEXT, NULL, (LPARAM) "");
        SendMessage(hEditObj, WM_SETTEXT, NULL, (LPARAM) "");
        SendMessage(hEditMsg, WM_SETTEXT, NULL, (LPARAM) "");
        MessageBox(NULL, TEXT("Message sent.\n"), TEXT(""),
                   MB_OK | MB_ICONINFORMATION);
        closesocket(server_desc);
      }
      return 1;
    }
    case RECV_CMD: {
      // login
      int ret = login(FALSE);
      if (ret == 0) {
        MessageBox(NULL, TEXT("Server offline"), NULL, MB_OK | MB_ICONERROR);
        closesocket(server_desc);
        return 1;
      }
      UpdateReceivedMsgs();
      hWndListView = CreateListView(hWnd);
      handle_error(hWndListView == NULL, "Unable to initialize list view.\n");
      ShowWindow(hEditMsg, SW_HIDE);
      ShowWindow(hEditTo, SW_HIDE);
      ShowWindow(hEditObj, SW_HIDE);
      ShowWindow(hSendButton, SW_HIDE);
      ShowWindow(hSyncCheck, SW_HIDE);
      ShowWindow(hGetButton, SW_HIDE);
      ShowWindow(hNewButton, SW_SHOW);
      ShowWindow(hShowButton, SW_SHOW);
      ShowWindow(hRemoveButton, SW_SHOW);
      ShowWindow(hBackButton, SW_HIDE);
      closesocket(server_desc);
      return 1;
    }
    case BACK_CMD: {
      hWndListView = CreateListView(hWnd);
      handle_error(hWndListView == NULL, "Unable to initialize list view.\n");
      Edit_SetText(hEditMsg, TEXT(""));
      Edit_SetText(hEditTo, TEXT(""));
      Edit_SetText(hEditObj, TEXT(""));
      Edit_SetReadOnly(hEditMsg, FALSE);
      Edit_SetReadOnly(hEditTo, FALSE);
      Edit_SetReadOnly(hEditObj, FALSE);
      SetWindowText(hLabelTo, TEXT("To:"));
      ShowWindow(hEditMsg, SW_HIDE);
      ShowWindow(hEditTo, SW_HIDE);
      ShowWindow(hEditObj, SW_HIDE);
      ShowWindow(hNewButton, SW_SHOW);
      ShowWindow(hShowButton, SW_SHOW);
      ShowWindow(hRemoveButton, SW_SHOW);
      ShowWindow(hBackButton, SW_HIDE);
      return 1;
    }
    case NEW_CMD: {
      ShowWindow(hWndListView, SW_HIDE);
      DestroyWindow(hWndListView);
      ShowWindow(hEditMsg, SW_SHOW);
      ShowWindow(hEditTo, SW_SHOW);
      ShowWindow(hEditObj, SW_SHOW);
      ShowWindow(hSendButton, SW_SHOW);
      ShowWindow(hSyncCheck, SW_SHOW);
      ShowWindow(hGetButton, SW_SHOW);
      ShowWindow(hNewButton, SW_HIDE);
      ShowWindow(hShowButton, SW_HIDE);
      ShowWindow(hRemoveButton, SW_HIDE);
      return 1;
    }
    case SHOW_CMD: {
      MSGINFO *msg;
      int ret;
      // get index
      int index =
          ListView_GetNextItem(hWndListView, -1, LVNI_FOCUSED | LVNI_SELECTED);
      if (index == -1)
        return 1;
      ShowWindow(hWndListView, SW_HIDE);
      DestroyWindow(hWndListView);
      ShowWindow(hEditMsg, SW_SHOW);
      ShowWindow(hEditTo, SW_SHOW);
      ShowWindow(hEditObj, SW_SHOW);
      ret = linked_list_get(msgs, index, (any_t *)&msg);
      handle_error(ret == LINKED_LIST_NOK, "Unable to retrieve messages,\n");
      Edit_SetText(hEditMsg, msg->msg);
      Edit_SetText(hEditTo, msg->from);
      Edit_SetText(hEditObj, msg->obj);
      SetWindowText(hLabelTo, TEXT("From:"));
      Edit_SetReadOnly(hEditMsg, TRUE);
      Edit_SetReadOnly(hEditTo, TRUE);
      Edit_SetReadOnly(hEditObj, TRUE);
      ShowWindow(hNewButton, SW_HIDE);
      ShowWindow(hShowButton, SW_HIDE);
      ShowWindow(hRemoveButton, SW_HIDE);
      ShowWindow(hBackButton, SW_SHOW);
      return 1;
    }
    case LOGIN_CMD: {
      BOOL isRegistering = IsDlgButtonChecked(hWindow, REGISTER_CMD);
      char isOk = login(isRegistering);
      if (isOk == 0) {
        if (!isRegistering)
          MessageBox(NULL, TEXT("Username o Psw Errate!\n"), NULL,
                     MB_OK | MB_ICONERROR);
        else
          MessageBox(NULL, TEXT("Username in uso!\n"), NULL,
                     MB_OK | MB_ICONERROR);
      } else if (isOk == 1) {
        loggedIn = TRUE;
        CheckDlgButton(hWnd, REGISTER_CMD, BST_UNCHECKED);
        ShowWindow(hEditMsg, SW_SHOW);
        ShowWindow(hSendButton, SW_SHOW);
        ShowWindow(hSyncCheck, SW_SHOW);
        ShowWindow(hGetButton, SW_SHOW);
        ShowWindow(hLoginButton, SW_HIDE);
        ShowWindow(hRegisterCheckBox, SW_HIDE);
        SetWindowText(hLabelTo, TEXT("To:"));
        SetWindowText(hLabelObj, TEXT("Obj:"));
        Edit_SetText(hEditTo, TEXT(""));
        Edit_SetText(hEditObj, TEXT(""));
        OpenMsgsFile();
        _TCHAR login_buf[3 * FIELD_LEN];
        _stprintf_s(login_buf, 3 * FIELD_LEN, TEXT("Client - %s"), username);
        SetWindowText(hWindow, login_buf);
      } else {
        MessageBox(NULL, TEXT("Errore Connessione.\n"), NULL,
                   MB_OK | MB_ICONERROR);
      }
      closesocket(server_desc);
      return 1;
    }
    case REGISTER_CMD: {
      BOOL checked = IsDlgButtonChecked(hWnd, REGISTER_CMD);
      if (checked)
        CheckDlgButton(hWnd, REGISTER_CMD, BST_UNCHECKED);
      else
        CheckDlgButton(hWnd, REGISTER_CMD, BST_CHECKED);
      return 1;
    }
    case SYNC_CMD: {
      BOOL checked = IsDlgButtonChecked(hWnd, SYNC_CMD);
      if (checked)
        CheckDlgButton(hWnd, SYNC_CMD, BST_UNCHECKED);
      else
        CheckDlgButton(hWnd, SYNC_CMD, BST_CHECKED);
      return 1;
    }
    case RM_CMD: {
      int ret;
      int ch;
      BOOL seenPlus = FALSE;
      // get index
      int index =
          ListView_GetNextItem(hWndListView, -1, LVNI_FOCUSED | LVNI_SELECTED);
      if (index == -1)
        return 1;
      ret = MessageBox(
          hWindow, TEXT("Do you want to remove selected message?\n"), TEXT(""),
          MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2 | MB_APPLMODAL);
      if (ret != IDYES)
        return 1;
      ret = linked_list_remove(msgs, index);
      if (ret == LINKED_LIST_NOK)
        MessageBox(NULL, TEXT("Unable to remove message.\n"), NULL,
                   MB_OK | MB_ICONERROR);
      fseek(userMsgs, 0, SEEK_SET);
      while ((ch = fgetc(userMsgs)) != EOF) {
        if (ch == '+') {
          seenPlus = TRUE;
          if (index == 0) {
            fseek(userMsgs, ftell(userMsgs) - 1, SEEK_SET);
            fputc('-', userMsgs);
            fseek(userMsgs, ftell(userMsgs), SEEK_SET);
          }
        } else if (ch == MSG_SEP && seenPlus) {
          index--;
          seenPlus = FALSE;
        }
      }
      ShowWindow(hWndListView, SW_HIDE);
      DestroyWindow(hWndListView);
      hWndListView = CreateListView(hWnd);
      handle_error(hWndListView == NULL, "Unable to initialize list view.\n");
    }
    }
  }
  default:
    return (DefWindowProc(hWnd, message, wParam, lParam));
  }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int CmdShow) {
  short int port; /*  port number               */
  //_TCHAR      buffer[MSG_LEN];     /*  Holds remote port         */
  struct hostent *he = NULL;
  u_long nRemoteAddr;
  WSADATA wsaData;

  struct _thread_info *thread_info = NULL;

  WNDCLASS wndclass;
  _TCHAR nome_applicazione[] = TEXT("Client");
  int ret;
  MSG msg;
  /*  Set the remote port  */
  port = SERVER_PORT;
  ret = WSAStartup(MAKEWORD(1, 1), &wsaData);
  handle_error(ret != 0, "error in WSAStartup()\n");

  /*  Set all bytes in socket address structure to
  zero, and fill in the relevant data members   */
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);

  /*  Set the remote IP address  */
  // nRemoteAddr = inet_addr(SERVER_ADDR);
  ret = InetPton(AF_INET, TEXT(SERVER_ADDR), &nRemoteAddr);
  handle_error(ret == 0, "client: IP address not valid.\n");

  servaddr.sin_addr.s_addr = nRemoteAddr;

  // connect and socket creation at login//

  wndclass.style = CS_HREDRAW | CS_VREDRAW;
  wndclass.lpfnWndProc = WndProc;
  wndclass.cbClsExtra = 0;
  wndclass.cbWndExtra = 0;
  wndclass.hInstance = NULL;
  wndclass.hIcon = LoadIcon(NULL, IDI_SHIELD);
  wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
  wndclass.hbrBackground = CreateSolidBrush(background);
  wndclass.lpszMenuName = NULL;
  wndclass.lpszClassName = nome_applicazione;

  ret = RegisterClass(&wndclass);
  handle_error(ret == 0, "Can't register class.\n");

  hWindow =
      CreateWindowEx(WS_EX_CLIENTEDGE, nome_applicazione, TEXT("Client"),
                     WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                     CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, NULL, NULL);
  handle_error(hWindow == INVALID_HANDLE_VALUE, "Can't create window.\n");
  ShowWindow(hWindow, SW_SHOW);
  UpdateWindow(hWindow);

  while (ret = GetMessage(&msg, NULL, 0, 0)) {
    if (ret == -1) {
      printf("event-message poll error\n");
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  WSACleanup();
  ExitProcess(0);
}
