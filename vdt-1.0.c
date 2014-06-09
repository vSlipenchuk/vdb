/*

 ISO-8601,Oracle:to_date,strfmt

 Для ускорения сканирования дат нужно придумать свою систему записи форматов.
 Она должна быть простой (побуквенной) и легко модифицируемой.
 Кроме нее (для соответствия) должна присутствовать система подобная Оракл to_date - менее быстрая,
 но вполне скоростная.
 Итак - символы:
  d,m,y,h,n,s,z,w - день, месяц(в т.ч. строковый),год, час, мин, сек и тайм-шифт,день недели
 все - остальное должно быть "аз-из" с текущим шаблоном. Игнорируются только пробельные симболы.
 Кажется-под это все ложится???
 Для нормального сканирования нужно уметь быстро забирать положительные числа. Делаем сканер с умножением?

 При сканировании - последовательно просматриваем паттерн. Как только весь шаблон подошел - счиатем все
 закончено-)))
 Имплементация манипуляций с датами
 Сканирование надо сделать нормальным. Как только дошли до конца маски - так все. Заканчивать.

 Причем - сканирование в первую очередь нужно делать в структуру tm - и только потом мапить это дело
 в другую какую-нить. Это позволит довольно оперативно переключаться между либами.
 Пишем сканер?

 struct tm
{
	int	tm_sec;		//ss, Seconds: 0-59 (K&R says 0-61?)
	int	tm_min;		// Minutes: 0-59
	int	tm_hour;	// Hours since midnight: 0-23
	int	tm_mday;	// Day of the month: 1-31
	int	tm_mon;		/// Months *since* january: 0-11
	int	tm_year;	// Years since 1900

	int	tm_wday;	// Days since Sunday (0-6)
	int	tm_yday;	// Days since Jan. 1: 0-365
	int	tm_isdst;	// +1 Daylight Savings Time, 0 No DST,

};


*/

#include "vdb.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define uchar unsigned char

int tzOffsetSeconds = +4*60*60; // Текущий оффсет в минутах от гринвича -)))
int tzOffsetSign = '+';
int tzOffsetHour = +4; //   Без знака
int tzOffsetMinutes = 0; // Без знака

/*
char *dateScanPattern[]={
    "dd.mm.yyyy hh24:mi:ss", // Русское время - дефолт для представления даты
    "yyyymmddThhnnssZ", // as signle string
    "yyyy-mm-dd hh24:mi:ss",
    "yyyy-mm-ddThh:nn:sstZ", // ISO - XML представление
    "DAY, dd MONTH yyyy hh24:mi:ssZ", //Wednesday, 30 January 2008 09:46:47 UTC
    "DY, dd MONTH yyyy hh24:mi:ssZ", // Wed, 30 Jan 2008 11:09:15 +0300
    0
    };

typedef struct { // 0=sec,1=min,2=hour,3=mday,4=mon,5=year,6=wday,7=yday
     char *patt; int shift;
     } date_patt;

date_patt datePatterns[]={
    "yyyy",5,"month",4, "hh24",2, "hh",2, "dd",3, "mm",4, "day", 6,
    0
    };
*/

int scanUINT(uchar *buf, int len, int *val) { // Переполнение?
int r=0; uchar ch; int v;
v=0;
while(len>0) {
    ch = buf[0]; if (ch<'0' || ch>'9') break;
    v=v*10+ch-'0'; r++; len--; buf++;
    }
if (r) *val=v;
return r;
}

int dt_year4(int year) {
  //printf("Year4 for %d\n",year);
  if(year>1000) return year;
  if(year>70) return year+1900;
  return year+2000;
}


int scanYear(uchar *buf,int len,int *val) {
int v;
if (len>4) len=4;
len = scanUINT(buf,len,&v);
if (len==2) { *val = dt_year4(v); return len;}// to 4 year ?
if (len==4) { *val=v; return len;}
return 0; // 3 letters year???
}


int scanYear4(uchar *buf,int len,int *val) {
int v;
if (len>4) len=4;
len = scanUINT(buf,len,&v);
if (len==4) { *val=v; return len;}
return 0; // 3 letters year???
}

uchar *szMonthName[] = {
        "January", "February", "Mart", "Aprile", "May", "June",
        "July", "August", "September", "October", "November", "December",0
    };

uchar *szMonName[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",0
    };

uchar *szWkName1[] = { // Имена дней недели = dt_weekday()
        "Err", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun",0
    };

#define szWkName (szWkName1+1)
//char *monNames[]="";

int dicIndex(uchar **dic,uchar *buf, int len,int *idx) { // try found this in a dic --?
uchar *str; int i,r;
for(r=0;*dic;dic++,r++) {
    for(i=0,str=*dic;i<len&&str[i]&&str[i]==buf[i];i++); // check all data
    //printf("i=%d for str=%s and buf=%s len=%d\n",i,str,buf,len);
    if (str[i]==0) { *idx=r; return i;} // ok, found, return length
    }
return 0;
}

uchar *szWDayName0[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat",0};
uchar *szWDayName1[] = { "Sunday","Monday", "Tuesday", "Wednesday", "Thurday", "Friday", "Saturday",0};

int scanWeekDay(uchar *buf,int len,int *val) { // days since SUN (Sun=0,Mon=1,..Sat=6)
int l;
l = dicIndex(szWDayName1,buf,len,val); if (l) return l;
l = dicIndex(szWDayName0,buf,len,val); if (l) return l;
return 0;
}

int scanMonth(uchar *buf,int len,int *val) {
int v,l;
l = len; if (l>2) l = 2;
l = scanUINT(buf,l,&v);
if (l) {
    if (v>=1 && v<=12) { *val=v-1; return l;} // ok
    return 0;
    }
l = dicIndex(szMonthName,buf,len,val); if (l) return l;
l = dicIndex(szMonName,buf,len,val); if (l) return l;
//printf("MoLen=%d for %s\n",len,buf);
return 0;
}

int scanDay(uchar *buf, int len, int *val) {
int v;
if (len>2) len = 2;
len = scanUINT(buf,len,&v);
if (len && v>0 && v<=32) { *val=v; return len;}
return 0;
}

/* RFC-2822

   EDT is semantically equivalent to -0400
   EST is semantically equivalent to -0500
   CDT is semantically equivalent to -0500
   CST is semantically equivalent to -0600
   MDT is semantically equivalent to -0600
   MST is semantically equivalent to -0700
   PDT is semantically equivalent to -0700
   PST is semantically equivalent to -0800

*/

int scanTZ(uchar *buf, int len, int *val) { // UTC,GMT,MSK&Other?
int sign=1,l,r=0,hh,nn;
if (buf[0]=='Z') { *val=0; return 1;}; // xml postfix Z
if (len>=3) {
    if (memcmp(buf,"UTC",3)==0) { buf+=3; len-=3; r+=3;}
     else if (memcmp(buf,"GMT",3)==0) { buf+=3; len-=3; r+=3;}
      //else if (memcmp(buf,"Z",1)==0) { buf++; len--; r++;}
    while(len>0 && buf[0]<=32) { len--; buf++; r++;}
    if (len==0) {*val=0; return r;} // Just GMT
    }
//printf("ScanTZ len=%d buf=%s\n",len,buf);
if (len==0) return 0;
if (buf[0]=='+') { buf++;len--;r++;}
 else if (buf[0]=='-') { buf++;len--;r++; sign=-1;}
  else return 0; // Must Be sign!
//printf("ScanTZ len=%d buf=%s\n",len,buf);
if (len==0) return 0; // no more
l = scanUINT(buf,len>2?2:len,&hh);
//printf("ScanTZ l=%d len=%d buf=%s nn=%d\n",l,len,buf,nn);
if (l!=2 || hh>23) return 0; // hours offset
//printf("ScanTZ len=%d buf=%s hh=%d\n",len,buf,hh);
r+=l; buf+=l; len-=l;
if (len==0) return 0;
if (buf[0]==':') {buf++; len--; r++;}; // 00:34 for instance -)))
if (len==0) return 0;
l = scanUINT(buf,len>2?2:len,&nn);
if (l!=2 || nn>59) return 0; // min offset
*val = sign*(hh*60+nn)*60; // minutes offset -)))
return r+l;
}

int scanTime(uchar *buf, int len, int *val) { // Складываем часы-минуты (милисекунды?) через ':'
int l,r=0,h,m,s=0,msec=0;
if (len<5) return 0;
l = scanUINT(buf,len>2?2:len,&h); if (l<1 || h>23) return 0;
buf+=l; len-=l; r+=l;
if (len==0 || buf[0]!=':') return 0;
buf++; len--; r++;
l = scanUINT(buf,len>2?2:len,&m); if (l<1 || m>59) return 0;
buf+=l; len-=l; r+=l;
if (len>0 && buf[0]==':') { // Секунды - опциональны.
    buf++; len--; r++;
    l = scanUINT(buf,len>2?2:len,&s); if (l!=2 || s>59) return 0;
    buf+=l; len-=l; r+=l;
    if (len>0 && buf[0]=='.') { // Милисекунды - опциональны
      l = scanUINT(buf,len>3?3:len,&msec); if (!l) return 0;
      buf+=l; len-=l; r+=l;
      }
    }
val[0]=h; val[1]=m; val[2]=s;
return r;
}

int scanUINT2(uchar *buf,int len,int *val,int max) {
if (len<2) return 0;
if (scanUINT(buf,2,val)!=2) return 0;
if (*val<0 || *val>=max) return 0;
return 2;
}

// 'z' - зона в большинстве случаев опциональна...
/*



  */
uchar *DateScanFormats[]={
    "y-m-dTtz",    // 2001-08-12T23:56:78[.908][+06:00] ; ISO-8601 short local and international (+06:00) formats
    "y-m-d tz",    // 2001-08-12T23:56:78[.908][+06:00] ; ISO-8601 short local and international (+06:00) formats
    "y-m-dz",      // 2001-08-12[+06:00] ; ISO-8601 short local and international (+06:00) formats
    "w m d t y",   // Sun Nov  6 08:49:37 1994 ; ANSI
    "w m d y h:n:s z", //Sun Apr 06 2008 00:00:00 GMT+0400 (Russian Daylight Time)
    "w, d-m-y tz", //Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
    "w, d m y tz", //Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
    "YmdThnsz", // YYYYMMNNHHNNSS
    "d.m.y t", // Rus Date&Time
    "d.m.y", // Rus Date - no time
    "tz",          // 23:34:45[+06:00] ; ISO-86-01 - время без даты, local & international
    0
    };

int dateScanF(uchar *buf,int len,uchar *fmt,int *Y) { // try to scan this fmt...
int r,tmp;
//printf("TryScan src=<%*.*s> and mask=%s\n",len,len,buf,fmt);
for(r=0;*fmt && len>0;) {
    int l=1;
    while(*fmt && *fmt<=32) fmt++;
    if (!*fmt) break;
    while(len>0 && buf[0]<=32) {buf++; len--;}
    if (len==0) break;
    //printf("fmt=%s in %*.*s\n",fmt,len,len,buf);
    switch(*fmt) {
     case 'y': l = scanYear(buf,len,Y+0);  break;
     case 'Y': l = scanYear4(buf,len,Y+0);  break;
     case 'm': l = scanMonth(buf,len,Y+1);
               //printf("MonScan=%d\n",l); getch();
               break;
     case 'd': l = scanDay(buf,len,Y+2);  break;
     case 'w': l = scanWeekDay(buf,len,&tmp);  break;
     case 't': l = scanTime(buf,len,Y+3); break;
     case 'h': l = scanUINT2(buf,len,Y+3,24); break;
     case 'n': l = scanUINT2(buf,len,Y+4,60); break;
     case 's': l = scanUINT2(buf,len,Y+5,60); break;
     case 'z': l = scanTZ(buf,len,Y+7);
                //printf("ScanTZ=%d shift=%d since '%s'\n",l,Y[7],buf);
                break; // if cant - nothing -)))
     default:  if (*buf!=*fmt) l=0; break;
     }
    //printf("Scan Res: %d\n",l);
    if (!l) if (*fmt!='z') return 0;
    r+=l; buf+=l; len-=l; fmt++;
    }
if (*fmt=='z') fmt++; // optional
if (!*fmt) return 1;
return 0;
}

int dateScanY(uchar *buf,int len, int *Y) { // Сканирует "что есть"
int l;
uchar **fmt = DateScanFormats;
for( ; *fmt; fmt++ ) { // check all formats one by one - )))
    while(len>0 && buf[0]<=32) {buf++;len--;} // ignore spaces
    memset(Y,0,sizeof(int)*8); // clearBuffer
    //printf("TryFmt:%s\n",fmt[0]);
    Y[7]=tzOffsetSeconds; // not used
    l =  dateScanF(buf,len,fmt[0],Y);
    if (l) {
        //printf("dateScan=%d for fmt=%s\n",l,buf);
        return l; // ok
        }
    }
return 0;
}

double dateScan(uchar *buf,int len) { // Куда сканировать-то будем? - нужны переменные!!!
double res;
int Y[8]; // Такой спец-массив
if (len<0) len=strlen(buf);
while(len>0 && buf[0]<=32) {len--; buf++;}
while(len>0 && buf[len-1]<=32) len--;
if (len==0 || !dateScanY(buf,len,Y)) return 0;
//printf("Hour=%d\n",Y[3]);
res = dt_encode(Y[0],Y[1]+1,Y[2],Y[3],Y[4],Y[5]); // ok?
if (!res) return 0;
//printf("Res=%lf correct=%d sec\n",res);
if (Y[7]!=tzOffsetSeconds)
   res-=(Y[7]-tzOffsetSeconds)/(24.*3600); // Need TimeShift --- TimeShift in seconds since TZ
//printf("Res2=%lf\n",res);
return res;
}

/*

date_time dt_scanf(char *B,int len) { return dateScan(B,len);}


void dtScanTest() {
uchar *ScanTest[]= {
     "12.11.1971", "12.11.1971",
     "1971-11-12", "12.11.1971",
     "2001-08-12+03:00","12.08.2001", // SameTZET
     "2001-08-12 10:00+02:00","12.08.2001 11:00", // SameTZET
     "Sunday, 06-Nov-94 08:49:37 GMT+0000","1994-11-06 11:49:37",// ; RFC 850, obsoleted by RFC 1036
     "Sun, 06 Nov 1994 08:49:37 GMT+03:00","6.11.94 8:49:37",//  ; RFC 822, updated by RFC 1123
     "1971-11-12Z+03:00","1971-11-12 03:00",//  ; RFC 822, updated by RFC 1123
     //"w, d m y tz", //Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123

     0
     };

uchar **s; double dt1,dt2;
for(s = ScanTest;*s;s+=2) { // Test Scans
    dt1 = dt_scanf(s[0],-1);
    //return 1;
    dt2 = dt_scanf(s[1],-1);
    if (dt_cmp(dt1,dt2)==0 && dt2) printf("+PASS '%s' equal '%s'\n",s[0],s[1]);
     else {
          printf("-FAILED %s not eq %s (%lf != %lf)\n",s[0],s[1],dt1,dt2);
          printf("dat1=%s\n",fmtDate(0,dt1));
          printf("dat2=%s\n",fmtDate(0,dt2));
          }
    }
}

*/


//-- Date&TimeRemaining
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

//typedef double date_time;

date_time dt_delta=0;

double Second=1.157407407407407e-5;
char   szdtfmt[200];


int dt_month_day[2][12]={{31,28,31,30,31,30,31,31,30,31,30,31},
                         {31,29,31,30,31,30,31,31,30,31,30,31}};
typedef char *pchar;
pchar   dt_mon_name0[13]={"","JAN","FEB","MAR","APR","MAY",
                         "JUN","JUL","AUG","SEP","OCT","NOV",
                         "DEC"};

pchar   dt_mon_name[13]={"","Jan","Feb","Mar","Apr","May",
                         "Jun","Jul","Aug","Sep","Oct","Nov",
                         "Dec"};

// Tшёюъюёэvщ ыш уюф ?
int dt_leap_year(int Year){  return (Year%4)==0 && (Year%100!=0 || Year%400==0); }

int dt_year4(int year);

int DaysInMonth(int year, int month) {
    if (month<1 || month>12) return 0; // fail?
    return dt_month_day[dt_leap_year(year)][month-1];
}


// -хэ№ эхфхыш
int dt_weekday(date_time strt) {
  double Day;  modf(strt,&Day);  return (((int)(Day+4+2))%7)+1;
}


// фэхщ ё 00.00.0000
date_time date_encode(int Year, int Month, int Day)
{
  int i,*MonDay;
  MonDay=dt_month_day[dt_leap_year(Year)]; // фэхщ т ьхё Ўрї(уюф юсvўэvщ/тшёюъ.)
  i= ( Year>=1 && Year<9999  &&  Month>=1 && Month<=12  &&
       Day>=1  && Day<=MonDay[Month-1] );  // трышфэюёЄ№ тїюфэvї фрээvї
  if(i)
  {
    for(i=0; i<Month-1; i++)  Day+=MonDay[i]; // фэхщ ё эрўрыр уюфр
    i=Year-1;
    return  i*365 + i/4 - i/100  + i/400 + Day - dt_delta;
  }
  return 0;
}


// ¦ЁхюсЁрчютрЄ№ тЁхь  т фюы¦ ёєЄюъ (double)
date_time time_encode(int Hour, int Min, int Sec) {
  if(Hour>=0 && Hour<24 && Min>=0 && Min<60 && Sec>=0 && Sec<60)
    return (Hour*3600+Min*60+Sec)/(24.*3600);
  return 0;
}


// ¦ЁхюсЁрчютрЄ№ фрЄє ш тЁхь  т фюы¦ ёєЄюъ (double)
date_time dt_encode(int Year, int Month, int Day, int Hour, int Min, int Sec) {
  return date_encode(Year,Month,Day) + time_encode(Hour,Min,Sec);
}


// -хыхэшх Ўхыvї
void DivMod(int Dividend, int Divisor, int *Result, int *Remainer) {
 /*  div_t divt=div(Dividend, Divisor);
  *Result=divt.quot;
  *Remainer=divt.rem;
*/
  *Result =  Dividend / Divisor;
  *Remainer = Dividend % Divisor;
}


// Lчтыхў№ Day.Month.Year шч яюыэющ фрЄv/тЁхьхэш Date
void date_decode(date_time Date, int *Year, int *Month, int *Day) {
  int
    D1 = 365,
    D4 = D1 * 4 + 1,
    D100 = D4 * 25 - 1,
    D400 = D100 * 4 + 1,
    Y, M, D, I, T,
    *DayTable;

  T=(int)Date;
  if(T<=0){ *Year=0; *Month=0; *Day=0; }
  else
  {
    for(T--,Y=1; T>=D400; T-=D400,Y+=400);              // ъєёъш яю 400 ыхЄ
    DivMod(T,D100,&I,&D);  if(I==4){I--;D+=D100;};  Y+=I*100; // яю 100 ыхЄ
    DivMod(D, D4,&I,&D);   Y+=I*4;                            // яю 4 уюфр
    DivMod(D, D1,&I,&D);   if(I==4){I--;D+=D1;};    Y+=I;     // яю уюфє

    DayTable = dt_month_day[dt_leap_year(Y)];  // фэхщ т ьхё Ўрї
    for(M = 1; 1; D-=I,M++){ I=DayTable[M-1];  if(D<I) break; }

    *Year=Y;   *Month=M;   *Day= D + 1;
  }
}


// Lчтыхў№ Hour:Min:Sec шч яюыэющ фрЄv/тЁхьхэш Date
void time_decode(date_time D, int *Hour, int *Min, int *Sec) {
  double Int;  int Secs;
  Secs=(int)(modf(D+Second/2,&Int)*24*3600);
  DivMod(Secs,3600,Hour,&Secs);
  DivMod(Secs,60,Min,&Secs);
  *Sec=Secs;
}


// Lчтыхў№ ёюёЄрты ¦•шх шч яюыэющ фрЄv/тЁхьхэш Date
int dt_decode(date_time DT,
        int *Year, int *Month, int *Day,
        int *Hour, int *Min,   int *Sec) {
date_decode(DT,Year,Month,Day); time_decode(DT,Hour,Min,Sec);
return *Year>0 && *Month>0 && *Day>0;
}


// LюЁьрЄv яЁхфёЄртыхэш  фрЄ/тЁхьхэ
struct{  char *In,*Out; int Shift; }  Fmdate_time[]=
{
  {"yyyy","%04d",1},
  {"mmm","%s",-1},
  {"mon","%s",-1},

  {"yy","%02d",0}, {"mm","%02d",2}, {"dd","%02d",3},
  {"hh24","%02d",4},
  {"hh","%02d",4},
   {"nn","%02d",5}, {"mi","%02d",5},
   {"ss","%02d",6},
  {"m","%d",2}, {"d","%d",3},
  {"h","%d",4}, {"n","%d",5}, {"s","%d",6},
  {0}
};


char *dt_str(char *Fmt, date_time D) {
  char *Buf=NULL;
  int   len=0;
  int Start[7],i,k,l=0; char *B=Buf;

  dt_decode(D, Start+1, Start+2, Start+3, Start+4, Start+5, Start+6);
  Start[0]=Start[1]%100; i=0;
  while(*Fmt)
  {
    if(l+40>len){ Buf=realloc(Buf,l+40); B=Buf+l; len=l+30;}
    for(i=0; i<13; i++)
      if(strncmp(Fmdate_time[i].In,Fmt,strlen(Fmdate_time[i].In))==0) break;
    if(i==13){ l++; *B=*Fmt; Fmt++; B++;}
    else
    {
      if(Fmdate_time[i].Shift==-1) k=sprintf(B,"%s",dt_mon_name[Start[2]]);
      else k=sprintf(B,Fmdate_time[i].Out,Start[Fmdate_time[i].Shift]);
      l+=k; B+=k; Fmt+=strlen(Fmdate_time[i].In);
    }
  }
  *B=0;
  return (Buf);
}

int buf_dt_str(uchar *Buf,int maxSize, char *Fmt, date_time D) {
//  int   len=0;
  int Start[7],i,k,l=0; char *B=Buf;

  dt_decode(D, Start+1, Start+2, Start+3, Start+4, Start+5, Start+6);
  Start[0]=Start[1]%100; i=0;
  while(*Fmt)
  {
    if(l+40>maxSize){ *B=0; return 0; } // No More Size
    for(i=0; i<13; i++)
      if(strncmp(Fmdate_time[i].In,Fmt,strlen(Fmdate_time[i].In))==0) break;
    if(i==13){ l++; *B=*Fmt; Fmt++; B++;}
    else
    {
      if(Fmdate_time[i].Shift==-1) k=sprintf(B,"%s",dt_mon_name[Start[2]]);
      else k=sprintf(B,Fmdate_time[i].Out,Start[Fmdate_time[i].Shift]);
      l+=k; B+=k; Fmt+=strlen(Fmdate_time[i].In);
    }
  }
  *B=0;
  return 1;
}


date_time dt_scanf0(char *B,int len) {
  int S[6],OK=0; char p = 0;
  if(len>0){ p=B[len];B[len]=0;}
  if(!OK)
  {
    memset(S,0,sizeof(int)*6);
    OK=(sscanf(B,"%d.%d.%d %d:%d:%d",S+2,S+1,S+0,S+3,S+4,S+5)>=3);
  }
  if(!OK)
  {
    memset(S,0,sizeof(int)*6);
    OK=(sscanf(B,"%04d%02d%02d%02d%02d%02d",S,S+1,S+2,S+3,S+4,S+5)>=3);
  }
  if(!OK)
  {
    memset(S,0,sizeof(int)*6);
    OK=(sscanf(B,"%d-%d-%d.%d.%d.%d",S,S+1,S+2,S+3,S+4,S+5)>=3);
  }
  if(len>0) B[len]=p;
  if(!OK) return 0;
  return dt_encode(dt_year4(S[0]),S[1],S[2],S[3],S[4],S[5]);
}


char *dt_gupta(date_time D) {
  int Year,Month,Day,Hour,Min,Sec;
  dt_decode(D,&Year,&Month,&Day,&Hour,&Min,&Sec);
  sprintf(szdtfmt,"%04d-%02d-%02d-%02d.%02d.%02d",Year,Month,Day,Hour,Min,Sec);
  return szdtfmt;
}


char *dt_rus(date_time D) {
  int Year,Month,Day,Hour,Min,Sec;
  dt_decode(D,&Year,&Month,&Day,&Hour,&Min,&Sec);
  sprintf(szdtfmt,"%02d.%02d.%04d %02d:%02d:%02d",Day,Month,Year,Hour,Min,Sec);
 return szdtfmt;
}


int dt_cmp(date_time D1, date_time D2) {
  double Res;
  Res=D1-D2;
  if(fabs(Res)<Second) return 0;
  if(Res<0) return -1;
  return 1;
}


/*
int dt_year4(int year) {
  //printf("Year4 for %d\n",year);
  if(year>1000) return year;
  if(year>70) return year+1900;
  return year+2000;
}
  */


date_time now() {
  time_t now; struct tm *tmnow;
  time(&now); tmnow=localtime(&now);
  return dt_encode(dt_year4(tmnow->tm_year),tmnow->tm_mon+1,tmnow->tm_mday,
               tmnow->tm_hour,tmnow->tm_min,tmnow->tm_sec);
}


date_time date() {
  time_t now; struct tm *tmnow;
  time(&now); tmnow=localtime(&now);
  return date_encode(dt_year4(tmnow->tm_year),tmnow->tm_mon+1,tmnow->tm_mday);
}


int dt2arr(double date, int *Y) { // Extracts date to 7 bytes array ->>>
return dt_decode(date, Y, Y+1,Y+2, Y+3,Y+4,Y+5); // Date&Time
}

int dt2tm(double date, struct tm *t) {
int Y[8]; //int code;
memset(t,0,sizeof(*t));
if (!dt2arr(date,Y)) return 0; // Extracts
t->tm_year = Y[0]-1900;
t->tm_mon = Y[1]-1; // from 0
t->tm_mday = Y[2];
t->tm_hour = Y[3];
t->tm_min = Y[4];
t->tm_sec = Y[5];
t->tm_wday = dt_weekday(date); // since Sunday=0
return 1;
}

char *dt2rfc822(double date, char *out) { // Конвертирует дату в rfc822  -> Wed, 18 Jun 2008 06:00:10 +0400
int Y[8];
if (!dt2arr(date,Y)) {  sprintf(out,"NULL");  return out; } // Extracts all info
//printf("WeekDay=%d\n", dt_weekday(date));
//sprintf(out,"Test");
//struct tm t;
//dt_tm(t,date); // Gets Tm Info
sprintf(out,"%s, %2d %s %d %02d:%02d:%02d %c%02d%02d", szWkName1[dt_weekday(date)], Y[2], szMonName[ Y[1]-1 ],  Y[0], Y[3],Y[4],Y[5],
  tzOffsetSign,tzOffsetHour, tzOffsetMinutes);
return out; //
}

void dt_init() {
//double d; char buf[80];
int tmp;
tzset();
//printf("tzName:%s timezone:%d daylight:%d\n", tzname,timezone/(3600),daylight);
//printf("tzOffsetSeconds: %d\n",tzOffsetSeconds);
tzOffsetSeconds = tmp = -timezone + daylight*3600;
if (tmp<0) { tzOffsetSign='-'; tmp=-tmp;} else tzOffsetSign='+';
tzOffsetHour = tmp/3600; tmp-=3600*tzOffsetHour;
tzOffsetMinutes = tmp/60;
//printf("tzOffsetSeconds: %d\n",tzOffsetSeconds);
//printf("Date: %s\n", dt2rfc822( now() , buf));
}
