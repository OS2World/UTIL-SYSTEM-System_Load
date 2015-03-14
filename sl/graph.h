#define INCL_DOSDATETIME
#include <os2.h>

/*
   Графики.
   --------

   Для графиков должен быть создан базовый объект. На его основе создаются
   один или несколько объектов для хранения данных. После заполнения временными
   метками (в базовом объекте) и соответствующими им значениями (в объекте(тах)
   для хранения данных) строится графическое представление данных.
   На одном графике могут быть отображены данные одного или нескольких
   хранилищ, т.е. может быть выведено одна или несколько кривых одновременно.

   Последовательность действий приложения:

   1. Инициализация базового объекта содержащего данные о шкале времени,
      глобальных минимумов и максимумов, амплитуде и пр.

      BOOL grInit(PGRAPH pGraph, ULONG ulValWindow, ULONG ulTimeWindow,
                  ULONG ulMin, ULONG ulMax);

      pGraph:
        Указатель на структуру GRAPH в пользовательской области данных.
      ulValWindow:
        Количество значений (или временных отметок) достаточное для заполнения
        всего графика.
      ulTimeWindow:
        Диапазон времени в милисекундах, который будет отображён на графике.

      Если предполагается, что график будет отображать значения полученные
      через равные промежутки времени dt (милисекунд) за период ulTimeWindow, 
      то ulValWindow = ulTimeWindow / dt;

      ulMin:
        Максмальный минимум. При отображении графиков за минимальное значение
        будет взято минимальное значение данных не превышающее ulMin.

      ulMax:
        Минимальный максимум. При отображении графиков за максимальное значение
        будет взято максималное значение данных не меньше ulMax.

      Если ulMin больше или равно ulMax, будет применяться автоматический
      расчёт значений минимумов и максимумов для каждого графического
      отображения данных.

          При автоматическом расчёте масштаб подбирается так, чтобы графики с
          минимальной амплитудой отличной от 0 гарантированно имели видимую
          разницу значений (не превращались в прямую). График с минимальной
          амплитудой будет занимать 25% высоты окна, с максимальной - 100%,
          графики с другими амплитудами будут пропорцио масштабироваться
          по оси ординат между четвертью и полной высотой области отображения.
          Это позволяет визуально отражать разницу амплитуд без "вытягивания"
          данных с минимальной амплитудой в прямую линию.

   2. Инициализация хранилищ значений. Эти объекты содержат значения измеряемых
      велечин в моменты времени хранящиеся в базовом объекте GRAPH.

      grInitVal(PGRAPH pGraph, PGRVAL pGrVal);

      pGraph:
        Указатель на структуру GRAPH в пользовательской области данных которая
        была инициализированна вызовом grInit().

      pGrVal:
        Указатель на инициализируемую структуру GRVAL в пользовательской
        области данных.

   На следующих шагах 3 и 4 фиксируются моменты времени и соответствующие им
   величины измеряемых значений. После 4го шага или серии шагов 3 и 4 данные
   могут быть отображены в графическом виде. Данные не должны отображаться если
   выполнен шаг 2, но не выполнен шаг 3 ДЛЯ ВСЕХ хранилищ данных значений
   отображаемых на графике.

   3. Фиксирование нового момента времени для которого имеются измеряемые
      значения. Этот шаг должен быть выполнен перед записью новых значений в
      хранилища данных.

      grNewTimestamp(PGRAPH pGraph, ULONG ulTimestamp);

      ulTimestamp:
        Время в миллисекундах для которого получены новые значения. Момент
        отсчёта времени значения не имеет. С каждым последующим вызовом ф-ии
        время должно увеличиваться - значение ulTimestamp больше предыдущего.

   4. Запись значений для момента времени установленного на шаге 3. Этот шаг
      должен быть выполнен для всех хранилищ значений после установки временной
      отметки на шаге 3. 

      grSetValue(PGRAPH pGraph, PGRVAL pGrVal, ULONG ulValue);      

      pGrVal:
        Указатель на структуру хранилища значений инициализированного для
        объекта pGraph вызовом grInitVal().

      ulValue:
        Измерянное значение.

   5. Графическое отображение значений. Могут быть отображены графики значений
      одного или сразу нескольких хранилищ значений.

      grDraw(PGRAPH pGraph, HPS hps, PRECTL prclGraph, ULONG cGrVal,
             PGRVAL *ppGrVal, PGRPARAM pParam);

      cGrVal:
        Кличество графиков которые будт отображены.

      ppGrVal:
        Массив указателей на хранилища данных.

      pParam:
        Указатель на структуру содержащую различные параметры отображения
        графиков.

   Если данные отображаются в реальном времени, можно перейти к шагу 3 для
   пополнения данных и последующего отображения изменений.
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
