#define INCL_DOSDATETIME
#include <os2.h>

/*
   ��䨪�.
   --------

   ��� ��䨪�� ������ ���� ᮧ��� ������ ��ꥪ�. �� ��� �᭮�� ᮧ������
   ���� ��� ��᪮�쪮 ��ꥪ⮢ ��� �࠭���� ������. ��᫥ ���������� �६���묨
   ��⪠�� (� ������� ��ꥪ�) � ᮮ⢥�����騬� �� ���祭�ﬨ (� ��ꥪ�(��)
   ��� �࠭���� ������) ��ந��� ����᪮� �।�⠢����� ������.
   �� ����� ��䨪� ����� ���� �⮡ࠦ��� ����� ������ ��� ��᪮�쪨�
   �࠭����, �.�. ����� ���� �뢥���� ���� ��� ��᪮�쪮 �ਢ�� �����६����.

   ��᫥����⥫쭮��� ����⢨� �ਫ������:

   1. ���樠������ �������� ��ꥪ� ᮤ�ঠ饣� ����� � 誠�� �६���,
      ��������� �����㬮� � ���ᨬ㬮�, ������㤥 � ��.

      BOOL grInit(PGRAPH pGraph, ULONG ulValWindow, ULONG ulTimeWindow,
                  ULONG ulMin, ULONG ulMax);

      pGraph:
        �����⥫� �� �������� GRAPH � ���짮��⥫�᪮� ������ ������.
      ulValWindow:
        ������⢮ ���祭�� (��� �६����� �⬥⮪) �����筮� ��� ����������
        �ᥣ� ��䨪�.
      ulTimeWindow:
        �������� �६��� � ����ᥪ㭤��, ����� �㤥� �⮡ࠦ� �� ��䨪�.

      �᫨ �।����������, �� ��䨪 �㤥� �⮡ࠦ��� ���祭�� ����祭��
      �१ ࠢ�� �஬���⪨ �६��� dt (����ᥪ㭤) �� ��ਮ� ulTimeWindow, 
      � ulValWindow = ulTimeWindow / dt;

      ulMin:
        ���ᬠ��� ������. �� �⮡ࠦ���� ��䨪�� �� �������쭮� ���祭��
        �㤥� ���� �������쭮� ���祭�� ������ �� �ॢ���饥 ulMin.

      ulMax:
        ��������� ���ᨬ�. �� �⮡ࠦ���� ��䨪�� �� ���ᨬ��쭮� ���祭��
        �㤥� ���� ���ᨬ����� ���祭�� ������ �� ����� ulMax.

      �᫨ ulMin ����� ��� ࠢ�� ulMax, �㤥� �ਬ������� ��⮬���᪨�
      ����� ���祭�� �����㬮� � ���ᨬ㬮� ��� ������� ����᪮��
      �⮡ࠦ���� ������.

          �� ��⮬���᪮� ����� ����⠡ �����ࠥ��� ⠪, �⮡� ��䨪� �
          �������쭮� ������㤮� �⫨筮� �� 0 ��࠭�஢���� ����� �������
          ࠧ���� ���祭�� (�� �ॢ�頫��� � �����). ��䨪 � �������쭮�
          ������㤮� �㤥� �������� 25% ����� ����, � ���ᨬ��쭮� - 100%,
          ��䨪� � ��㣨�� ������㤠�� ���� �ய��樮 ����⠡�஢�����
          �� �� �न��� ����� �⢥���� � ������ ���⮩ ������ �⮡ࠦ����.
          �� �������� ���㠫쭮 ��ࠦ��� ࠧ���� ������� ��� "��������"
          ������ � �������쭮� ������㤮� � ����� �����.

   2. ���樠������ �࠭���� ���祭��. �� ��ꥪ�� ᮤ�ঠ� ���祭�� �����塞��
      ����稭 � ������� �६��� �࠭�騥�� � ������� ��ꥪ� GRAPH.

      grInitVal(PGRAPH pGraph, PGRVAL pGrVal);

      pGraph:
        �����⥫� �� �������� GRAPH � ���짮��⥫�᪮� ������ ������ �����
        �뫠 ���樠����஢���� �맮��� grInit().

      pGrVal:
        �����⥫� �� ���樠�����㥬�� �������� GRVAL � ���짮��⥫�᪮�
        ������ ������.

   �� ᫥����� 蠣�� 3 � 4 䨪������� ������� �६��� � ᮮ⢥�����騥 ��
   ����稭� �����塞�� ���祭��. ��᫥ 4�� 蠣� ��� �ਨ 蠣�� 3 � 4 �����
   ����� ���� �⮡ࠦ��� � ����᪮� ����. ����� �� ������ �⮡ࠦ����� �᫨
   �믮���� 蠣 2, �� �� �믮���� 蠣 3 ��� ���� �࠭���� ������ ���祭��
   �⮡ࠦ����� �� ��䨪�.

   3. ����஢���� ������ ������ �६��� ��� ���ண� ������� �����塞�
      ���祭��. ��� 蠣 ������ ���� �믮���� ��। ������� ����� ���祭�� �
      �࠭���� ������.

      grNewTimestamp(PGRAPH pGraph, ULONG ulTimestamp);

      ulTimestamp:
        �६� � �����ᥪ㭤�� ��� ���ண� ����祭� ���� ���祭��. ������
        ������ �६��� ���祭�� �� �����. � ����� ��᫥���騬 �맮��� �-��
        �६� ������ 㢥��稢����� - ���祭�� ulTimestamp ����� �।��饣�.

   4. ������ ���祭�� ��� ������ �६��� ��⠭��������� �� 蠣� 3. ��� 蠣
      ������ ���� �믮���� ��� ��� �࠭���� ���祭�� ��᫥ ��⠭���� �६�����
      �⬥⪨ �� 蠣� 3. 

      grSetValue(PGRAPH pGraph, PGRVAL pGrVal, ULONG ulValue);      

      pGrVal:
        �����⥫� �� �������� �࠭���� ���祭�� ���樠����஢������ ���
        ��ꥪ� pGraph �맮��� grInitVal().

      ulValue:
        �����ﭭ�� ���祭��.

   5. ����᪮� �⮡ࠦ���� ���祭��. ����� ���� �⮡ࠦ��� ��䨪� ���祭��
      ������ ��� �ࠧ� ��᪮�쪨� �࠭���� ���祭��.

      grDraw(PGRAPH pGraph, HPS hps, PRECTL prclGraph, ULONG cGrVal,
             PGRVAL *ppGrVal, PGRPARAM pParam);

      cGrVal:
        �����⢮ ��䨪�� ����� ��� �⮡ࠦ���.

      ppGrVal:
        ���ᨢ 㪠��⥫�� �� �࠭���� ������.

      pParam:
        �����⥫� �� �������� ᮤ�ঠ��� ࠧ���� ��ࠬ���� �⮡ࠦ����
        ��䨪��.

   �᫨ ����� �⮡ࠦ����� � ॠ�쭮� �६���, ����� ��३� � 蠣� 3 ���
   ���������� ������ � ��᫥���饣� �⮡ࠦ���� ���������.
*/

#define GRAPG_ORDINATE_LEFT_PAD			5
#define GRAPG_ABSCISSA_TOP_PAD			5
#define GRAPG_ABSCISSA_BOTTOM_PAD		1
#define GRAPG_ORDINATE_CAPTION_VPAD		5
#define GRAPG_ABSCISSA_BOTTOM_CAPTION_RPAD	5

#define GRPF_MIN_LABEL		1
#define GRPF_MAX_LABEL		2
#define GRPF_TIME_LABEL		4
#define GRPF_LEFT_TOP_CAPTION	8

typedef struct _GRVALPARAM {
  LONG		clrGraph;
  ULONG		ulLineWidth; // 0 - Fill graph, Other - line graph
} GRVALPARAM, *PGRVALPARAM;

typedef struct _GRPARAM {
  ULONG		ulFlags;
  PSZ		pszAbscissaTopCaption;
  PSZ		pszAbscissaBottomCaption;
  PSZ		pszOrdinateCaption;
  // fnValToStr() - optional. Callback function to make max. & min. ordinate
  // values captions
  LONG (*fnValToStr)(ULONG ulVal, PCHAR pcBuf, ULONG cbBuf);
  ULONG		ulBorderCX;
  ULONG		ulBorderCY;
  LONG		clrBorder;
  LONG		clrBackground;
  ULONG		cParamVal;
  PGRVALPARAM   pParamVal;
} GRPARAM, *PGRPARAM;

typedef struct _GRAPH {
  ULONG		ulTimeWindow;
  ULONG		ulInitMin;
  ULONG		ulInitMax;
  ULONG		ulMin;
  ULONG		ulMax;
  ULONG		ulMinAmp;
  ULONG		ulMaxAmp;
  DATETIME	stDateTime;

  ULONG		ulValWindow;
  LONG		cTimestamps;
  PULONG	pulTimestamps;
  LONG		lLastIndex;
} GRAPH, *PGRAPH;

typedef struct _GRVAL {
  ULONG		ulMin;
  ULONG		ulMax;

  ULONG		ulValWindow;
  LONG		cValues;
  PULONG	pulValues;
  LONG		lLastIndex;
} GRVAL, *PGRVAL;


BOOL grInit(PGRAPH pGraph, ULONG ulValWindow, ULONG ulTimeWindow,
            ULONG ulMin, ULONG ulMax);
VOID grDone(PGRAPH pGraph);
BOOL grSetTimeScale(PGRAPH pGraph, ULONG ulValWindow, ULONG ulTimeWindow);
VOID grNewTimestamp(PGRAPH pGraph, ULONG ulTimestamp);
BOOL grGetTimestamp(PGRAPH pGraph, PULONG pulTimestamp);
VOID grInitVal(PGRAPH pGraph, PGRVAL pGrVal);
VOID grDoneVal(PGRVAL pGrVal);
VOID grResetVal(PGRAPH pGraph, PGRVAL pGrVal);
VOID grSetValue(PGRAPH pGraph, PGRVAL pGrVal, ULONG ulValue);
BOOL grGetValue(PGRAPH pGraph, PGRVAL pGrVal, PULONG pulValue);
VOID grDraw(PGRAPH pGraph, HPS hps, PRECTL prclGraph, ULONG cGrVal,
            PGRVAL *ppGrVal, PGRPARAM pParam);
