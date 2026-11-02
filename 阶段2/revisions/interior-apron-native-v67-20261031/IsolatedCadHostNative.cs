// Stage1 orchestration only. No COM, no input injection, no SwitchDesktop,
// no SetForegroundWindow/ShowWindow, and no AutoCAD drawing API.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace CodexStage1Isolation {
    public sealed class WindowRecord {
        public long Hwnd, Owner, Root;
        public uint Pid, Thread;
        public string ClassName, Title, ThreadDesktop, EnumerationDesktop;
        public uint PidAtEnumeration, ThreadAtEnumeration;
        public bool Visible, ThreadDesktopKnown;
        public int ClientWidth, ClientHeight, ThreadDesktopError;
    }
    public sealed class DesktopInspection {
        public string DesktopName, Executable, CreationFileTime;
        public uint ProcessId, ExpectedThread;
        public bool PidFound, ExpectedThreadFound;
        public WindowRecord[] Windows;
    }
    public sealed class StartupTipAction {
        public string Utc, DesktopName, ClassName, Title, Status;
        public uint ProcessId, Thread;
        public long Hwnd;
        public bool Matched, Posted;
        public int Error;
    }
    public sealed class EventRecord {
        public long Sequence, Hwnd, Owner, Root;
        public uint Event, Pid, EventThread, EventTick;
        public int ObjectId, ChildId;
        public string Utc, Name, ClassName, ProcessImage, MonitorDesktop;
        public bool Visible;
    }
    public static class Native {
        internal const uint DESKTOP_READOBJECTS=1, DESKTOP_CREATEWINDOW=2, DESKTOP_WRITEOBJECTS=0x80;
        internal const uint DESKTOP_ENUMERATE=0x40, DESKTOP_CREATEMENU=4;
        internal const uint QUERY_LIMITED=0x1000, SYNCHRONIZE=0x100000, TERMINATE=1;
        internal const uint WAIT_OBJECT_0=0, WAIT_TIMEOUT=258, WAIT_FAILED=0xffffffff;
        internal delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr extra);
        internal delegate void WinEventProc(IntPtr hook,uint ev,IntPtr hwnd,int obj,int child,uint thread,uint time);
        [StructLayout(LayoutKind.Sequential)] internal struct Point { public int X,Y; }
        [StructLayout(LayoutKind.Sequential)] internal struct Rect { public int Left,Top,Right,Bottom; }
        [StructLayout(LayoutKind.Sequential)] internal struct Msg { public IntPtr Hwnd; public uint Message; public UIntPtr WParam; public IntPtr LParam; public uint Time; public Point Point; public uint Private; }
        [StructLayout(LayoutKind.Sequential)] internal struct FileTime { public uint Low,High; }
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] internal struct StartupInfo {
            public uint Cb; public string Reserved,Desktop,Title; public uint X,Y,XSize,YSize,XCount,YCount,Fill,Flags;
            public ushort ShowWindow,Reserved2; public IntPtr ReservedPtr,StdInput,StdOutput,StdError;
        }
        [StructLayout(LayoutKind.Sequential)] internal struct ProcessInfo { public IntPtr Process,Thread; public uint ProcessId,ThreadId; }
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] internal static extern IntPtr CreateDesktopW(string name,IntPtr device,IntPtr devmode,uint flags,uint access,IntPtr security);
        [DllImport("user32.dll",SetLastError=true)] internal static extern bool CloseDesktop(IntPtr desktop);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] internal static extern IntPtr OpenDesktopW(string name,uint flags,bool inherit,uint access);
        [DllImport("user32.dll",SetLastError=true)] internal static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
        [DllImport("user32.dll",SetLastError=true)] internal static extern IntPtr GetThreadDesktop(uint id);
        [DllImport("user32.dll")] internal static extern IntPtr GetProcessWindowStation();
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] internal static extern bool GetUserObjectInformationW(IntPtr obj,int index,StringBuilder data,uint bytes,out uint needed);
        [DllImport("user32.dll",SetLastError=true)] internal static extern bool EnumDesktopWindows(IntPtr desktop,EnumWindowsProc callback,IntPtr extra);
        [DllImport("user32.dll")] internal static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] internal static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
        [DllImport("user32.dll")] internal static extern bool IsWindowVisible(IntPtr hwnd);
        [DllImport("user32.dll")] internal static extern IntPtr GetAncestor(IntPtr hwnd,uint flag);
        [DllImport("user32.dll")] internal static extern IntPtr GetWindow(IntPtr hwnd,uint command);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] internal static extern int GetClassNameW(IntPtr hwnd,StringBuilder value,int capacity);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] internal static extern int GetWindowTextW(IntPtr hwnd,StringBuilder value,int capacity);
        [DllImport("user32.dll")] internal static extern bool GetClientRect(IntPtr hwnd,out Rect value);
        [DllImport("user32.dll",SetLastError=true)] internal static extern IntPtr SetWinEventHook(uint min,uint max,IntPtr module,WinEventProc callback,uint pid,uint thread,uint flags);
        [DllImport("user32.dll",SetLastError=true)] internal static extern bool UnhookWinEvent(IntPtr hook);
        [DllImport("user32.dll",SetLastError=true)] internal static extern int GetMessageW(out Msg msg,IntPtr hwnd,uint min,uint max);
        [DllImport("user32.dll",SetLastError=true)] internal static extern bool PeekMessageW(out Msg msg,IntPtr hwnd,uint min,uint max,uint remove);
        [DllImport("user32.dll")] internal static extern bool TranslateMessage(ref Msg msg);
        [DllImport("user32.dll")] internal static extern IntPtr DispatchMessageW(ref Msg msg);
        [DllImport("user32.dll",SetLastError=true)] internal static extern bool PostThreadMessageW(uint thread,uint msg,UIntPtr w,IntPtr l);
        [DllImport("user32.dll",SetLastError=true)] internal static extern bool PostMessageW(IntPtr hwnd,uint msg,UIntPtr w,IntPtr l);
        [DllImport("kernel32.dll")] internal static extern uint GetCurrentThreadId();
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] internal static extern bool CreateProcessW(string application,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,bool inherit,uint flags,IntPtr environment,string directory,ref StartupInfo startup,out ProcessInfo process);
        [DllImport("kernel32.dll",SetLastError=true)] internal static extern bool CloseHandle(IntPtr value);
        [DllImport("kernel32.dll",SetLastError=true)] internal static extern uint WaitForSingleObject(IntPtr value,uint ms);
        [DllImport("kernel32.dll",SetLastError=true)] internal static extern bool GetExitCodeProcess(IntPtr process,out uint code);
        [DllImport("kernel32.dll",SetLastError=true)] internal static extern bool GetProcessTimes(IntPtr process,out FileTime created,out FileTime exit,out FileTime kernel,out FileTime user);
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] internal static extern bool QueryFullProcessImageNameW(IntPtr process,uint flags,StringBuilder value,ref uint chars);
        [DllImport("kernel32.dll",SetLastError=true)] internal static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
        [DllImport("kernel32.dll",SetLastError=true)] internal static extern bool TerminateProcess(IntPtr process,uint code);
        internal static Exception Error(string what) { return new Win32Exception(Marshal.GetLastWin32Error(),what); }
        internal static string ObjectName(IntPtr handle) {
            if(handle==IntPtr.Zero) throw Error("null USER object");
            StringBuilder value=new StringBuilder(512); uint needed;
            if(!GetUserObjectInformationW(handle,2,value,(uint)value.Capacity*2,out needed)) throw Error("USER object name");
            return value.ToString();
        }
        public static string InputDesktopName() {
            IntPtr handle=OpenInputDesktop(0,false,DESKTOP_READOBJECTS);
            if(handle==IntPtr.Zero) throw Error("OpenInputDesktop");
            try { return ObjectName(handle); } finally { CloseDesktop(handle); }
        }
        public static string ThreadDesktopName(uint thread) { return ObjectName(GetThreadDesktop(thread)); }
        public static string CurrentThreadDesktopName() { return ThreadDesktopName(GetCurrentThreadId()); }
        public static string WindowStationName() { return ObjectName(GetProcessWindowStation()); }
        internal static string ProcessCreation(IntPtr process) {
            FileTime a,b,c,d; if(!GetProcessTimes(process,out a,out b,out c,out d)) throw Error("GetProcessTimes");
            ulong value=((ulong)a.High<<32)|a.Low;return value.ToString(CultureInfo.InvariantCulture);
        }
        internal static string ProcessImage(IntPtr process) {
            StringBuilder s=new StringBuilder(32768);uint n=(uint)s.Capacity;
            if(!QueryFullProcessImageNameW(process,0,s,ref n)) throw Error("QueryFullProcessImageName");return Path.GetFullPath(s.ToString());
        }
        internal static string TryProcessImage(uint pid) {
            IntPtr handle=OpenProcess(QUERY_LIMITED,false,pid);if(handle==IntPtr.Zero)return null;
            try { return ProcessImage(handle); }catch{return null;}finally{CloseHandle(handle);}
        }
        internal static string ClassName(IntPtr hwnd) { StringBuilder s=new StringBuilder(512);GetClassNameW(hwnd,s,s.Capacity);return s.ToString(); }
        internal static WindowRecord Window(IntPtr hwnd,bool title) {
            WindowRecord r=new WindowRecord();r.Hwnd=hwnd.ToInt64();r.Thread=GetWindowThreadProcessId(hwnd,out r.Pid);
            r.Root=GetAncestor(hwnd,2).ToInt64();r.Owner=GetWindow(hwnd,4).ToInt64();r.ClassName=ClassName(hwnd);r.Visible=IsWindowVisible(hwnd);
            if(title){StringBuilder s=new StringBuilder(4096);GetWindowTextW(hwnd,s,s.Capacity);r.Title=s.ToString();}
            try{r.ThreadDesktop=ThreadDesktopName(r.Thread);r.ThreadDesktopKnown=true;}
            catch(Win32Exception e){r.ThreadDesktop=null;r.ThreadDesktopError=e.NativeErrorCode;}
            catch{r.ThreadDesktop=null;r.ThreadDesktopError=-1;}
            Rect rect;if(GetClientRect(hwnd,out rect)){r.ClientWidth=rect.Right-rect.Left;r.ClientHeight=rect.Bottom-rect.Top;}return r;
        }
        public static WindowRecord Foreground() { return Window(GetForegroundWindow(),false); }
        public static string Quote(string value) {
            if(value==null)throw new ArgumentNullException("value");StringBuilder s=new StringBuilder("\"");int slashes=0;
            foreach(char c in value){if(c=='\\'){slashes++;continue;}if(c=='\"'){s.Append('\\',slashes*2+1);s.Append(c);slashes=0;continue;}s.Append('\\',slashes);slashes=0;s.Append(c);}
            s.Append('\\',slashes*2);s.Append('"');return s.ToString();
        }
        public static bool IdentityMatches(string actualImage,string actualCreated,string actualDesktop,string expectedImage,string expectedCreated,string expectedDesktop) {
            return String.Equals(Path.GetFullPath(actualImage),Path.GetFullPath(expectedImage),StringComparison.OrdinalIgnoreCase)
                && actualCreated==expectedCreated && actualDesktop==expectedDesktop;
        }
        internal static void RequirePrivateName(string value) {
            if(value==null||value.Length!=40||!value.StartsWith("CodexS1_",StringComparison.Ordinal))throw new ArgumentException("expected nonce private desktop required");
            for(int i=8;i<value.Length;i++)if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))throw new ArgumentException("private desktop nonce must be lowercase 32 hex");
        }
        internal static WindowRecord[] DesktopWindows(IntPtr desktop,string name,uint wantedPid) {
            List<WindowRecord> values=new List<WindowRecord>();
            EnumWindowsProc cb=delegate(IntPtr hwnd,IntPtr ignored){
                uint pid;uint tid=GetWindowThreadProcessId(hwnd,out pid);
                if(pid==wantedPid){WindowRecord r=Window(hwnd,true);r.EnumerationDesktop=name;r.PidAtEnumeration=pid;r.ThreadAtEnumeration=tid;values.Add(r);}return true;
            };
            if(!EnumDesktopWindows(desktop,cb,IntPtr.Zero))throw Error("EnumDesktopWindows private desktop");GC.KeepAlive(cb);return values.ToArray();
        }
        internal static DesktopInspection InspectOpenProcess(IntPtr process,string desktopName,uint pid,uint expectedThread) {
            RequirePrivateName(desktopName);
            IntPtr desk=OpenDesktopW(desktopName,0,false,DESKTOP_READOBJECTS|DESKTOP_ENUMERATE);
            if(desk==IntPtr.Zero)throw Error("OpenDesktop expected private desktop");
            try{
                if(ObjectName(desk)!=desktopName)throw new InvalidOperationException("opened private desktop name differs");
                DesktopInspection result=new DesktopInspection();result.DesktopName=desktopName;result.ProcessId=pid;result.ExpectedThread=expectedThread;
                result.Executable=ProcessImage(process);result.CreationFileTime=ProcessCreation(process);result.Windows=DesktopWindows(desk,desktopName,pid);
                foreach(WindowRecord r in result.Windows){if(r.PidAtEnumeration==pid){result.PidFound=true;if(expectedThread!=0&&r.ThreadAtEnumeration==expectedThread)result.ExpectedThreadFound=true;}}
                return result;
            }finally{CloseDesktop(desk);}
        }
        public static DesktopInspection InspectOwnedDesktop(string desktopName,uint pid,uint expectedThread) {
            IntPtr handle=OpenProcess(QUERY_LIMITED|SYNCHRONIZE,false,pid);if(handle==IntPtr.Zero)throw Error("open process for read-only private desktop inspection");
            try{return InspectOpenProcess(handle,desktopName,pid,expectedThread);}finally{CloseHandle(handle);}
        }
        public static uint StopVerified(uint pid,uint initialThread,string expectedImage,string expectedCreated,string expectedDesktop) {
            RequirePrivateName(expectedDesktop);
            IntPtr handle=OpenProcess(QUERY_LIMITED|SYNCHRONIZE|TERMINATE,false,pid);if(handle==IntPtr.Zero)throw Error("open owned worker for explicit stop");
            try {
                if(WaitForSingleObject(handle,0)==WAIT_OBJECT_0)return 0;
                if(initialThread==0)throw new InvalidOperationException("an explicitly attested worker GUI thread is required; no termination");
                DesktopInspection proof=InspectOpenProcess(handle,expectedDesktop,pid,initialThread);
                if(!proof.PidFound||!proof.ExpectedThreadFound||!IdentityMatches(proof.Executable,proof.CreationFileTime,proof.DesktopName,expectedImage,expectedCreated,expectedDesktop))throw new InvalidOperationException("owned process exe/time/private-desktop enumeration identity mismatch; no termination");
                if(!TerminateProcess(handle,70))throw Error("terminate verified owned worker");
                uint wait=WaitForSingleObject(handle,10000);if(wait!=WAIT_OBJECT_0)throw new InvalidOperationException("owned worker stop not completed");
                uint code;if(!GetExitCodeProcess(handle,out code))throw Error("owned stopped exit code");return code;
            }finally{CloseHandle(handle);}
        }
    }

    public sealed class EventMonitor : IDisposable {
        readonly object gate=new object();readonly Queue<EventRecord> events=new Queue<EventRecord>();
        readonly Dictionary<long,uint> windowPids=new Dictionary<long,uint>();readonly ManualResetEvent started=new ManualResetEvent(false);
        readonly Native.WinEventProc callback;GCHandle keepAlive;Thread thread;uint threadId;IntPtr foregroundHook,windowHook;long sequence;
        public bool Started,Stopped;public string Error,DesktopName;public int Dropped;const int Capacity=20000;
        public EventMonitor(){callback=OnEvent;}
        public void Start(){thread=new Thread(Pump);thread.IsBackground=true;thread.Start();if(!started.WaitOne(5000)||!Started)throw new InvalidOperationException("WinEvent monitor failed: "+Error);}
        void Pump(){
            try{
                threadId=Native.GetCurrentThreadId();DesktopName=Native.CurrentThreadDesktopName();keepAlive=GCHandle.Alloc(callback);Native.Msg seed;Native.PeekMessageW(out seed,IntPtr.Zero,0,0,0);
                foregroundHook=Native.SetWinEventHook(3,3,IntPtr.Zero,callback,0,0,0);
                windowHook=Native.SetWinEventHook(0x8000,0x8003,IntPtr.Zero,callback,0,0,0);
                if(foregroundHook==IntPtr.Zero||windowHook==IntPtr.Zero)throw Native.Error("SetWinEventHook");
                Started=true;started.Set();Native.Msg msg;int status;
                while((status=Native.GetMessageW(out msg,IntPtr.Zero,0,0))>0){Native.TranslateMessage(ref msg);Native.DispatchMessageW(ref msg);}
                if(status<0)throw Native.Error("monitor GetMessage");
            }catch(Exception e){Error=e.ToString();started.Set();}
            finally{
                if(foregroundHook!=IntPtr.Zero&&!Native.UnhookWinEvent(foregroundHook))Error=(Error??"")+"; foreground unhook failed";
                if(windowHook!=IntPtr.Zero&&!Native.UnhookWinEvent(windowHook))Error=(Error??"")+"; window unhook failed";
                if(keepAlive.IsAllocated)keepAlive.Free();Stopped=true;
            }
        }
        void OnEvent(IntPtr hook,uint ev,IntPtr hwnd,int obj,int child,uint evThread,uint tick){
            try{
                if(hwnd==IntPtr.Zero||((ev>=0x8000)&&(obj!=0||child!=0)))return;
                EventRecord r=new EventRecord();r.Sequence=Interlocked.Increment(ref sequence);r.Utc=DateTime.UtcNow.ToString("o");r.Event=ev;r.Hwnd=hwnd.ToInt64();r.EventThread=evThread;r.EventTick=tick;r.ObjectId=obj;r.ChildId=child;r.MonitorDesktop=DesktopName;
                Native.GetWindowThreadProcessId(hwnd,out r.Pid);
                if(r.Pid!=0)windowPids[r.Hwnd]=r.Pid;else if(windowPids.ContainsKey(r.Hwnd))r.Pid=windowPids[r.Hwnd];
                r.Owner=Native.GetWindow(hwnd,4).ToInt64();r.Root=Native.GetAncestor(hwnd,2).ToInt64();r.ClassName=Native.ClassName(hwnd);r.Visible=Native.IsWindowVisible(hwnd);r.ProcessImage=Native.TryProcessImage(r.Pid);
                r.Name=ev==3?"foreground":ev==0x8000?"create":ev==0x8001?"destroy":ev==0x8002?"show":"hide";
                lock(gate){if(events.Count>=Capacity){Dropped++;}else events.Enqueue(r);}
            }catch(Exception e){lock(gate){Error=(Error??"")+"; callback: "+e.Message;}}
        }
        public EventRecord[] Drain(){lock(gate){EventRecord[] result=events.ToArray();events.Clear();return result;}}
        public void Stop(){if(thread!=null&&thread.IsAlive){if(!Native.PostThreadMessageW(threadId,0x0012,UIntPtr.Zero,IntPtr.Zero))Error=(Error??"")+"; WM_QUIT post failed";if(!thread.Join(5000))Error=(Error??"")+"; monitor thread did not exit";}}
        public void Dispose(){Stop();started.Dispose();}
    }

    public sealed class DesktopWorker : IDisposable {
        IntPtr desktop,process,initialThread;public string DesktopName,DesktopPath,CommandLine,Image,CreationFileTime,CreationUtc;
        public uint ProcessId,InitialThreadId;public bool Created,DesktopClosed,ProcessHandlesClosed;
        public long DesktopHandle {get{return desktop.ToInt64();}}
        public DesktopWorker(string desktopName){
            if(String.IsNullOrEmpty(desktopName)||desktopName.IndexOf('\\')>=0)throw new ArgumentException("invalid private desktop name");
            if(!String.Equals(Native.WindowStationName(),"WinSta0",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("worker creation requires the existing interactive WinSta0 station");
            DesktopName=desktopName;DesktopPath="WinSta0\\"+desktopName;
            IntPtr existing=Native.OpenDesktopW(desktopName,0,false,Native.DESKTOP_READOBJECTS);
            if(existing!=IntPtr.Zero){Native.CloseDesktop(existing);throw new InvalidOperationException("nonce desktop already exists; use a fresh nonce");}
            int openError=Marshal.GetLastWin32Error();if(openError!=2&&openError!=3)throw new Win32Exception(openError,"cannot prove nonce desktop is new");
            desktop=Native.CreateDesktopW(desktopName,IntPtr.Zero,IntPtr.Zero,0,Native.DESKTOP_READOBJECTS|Native.DESKTOP_CREATEWINDOW|Native.DESKTOP_WRITEOBJECTS|Native.DESKTOP_ENUMERATE|Native.DESKTOP_CREATEMENU,IntPtr.Zero);
            if(desktop==IntPtr.Zero)throw Native.Error("CreateDesktop");
            if(Native.ObjectName(desktop)!=desktopName){Native.CloseDesktop(desktop);desktop=IntPtr.Zero;throw new InvalidOperationException("created desktop name differs");}
        }
        public static string BuildCadCommandLine(string acad,string profile,string template,string boot,string earlyModule){
            return Native.Quote(acad)+" /nologo /p "+Native.Quote(profile)+" /t "+Native.Quote(template)+(String.IsNullOrEmpty(earlyModule)?"":" /ld "+Native.Quote(earlyModule))+" /b "+Native.Quote(boot);
        }
        public void Launch(string acad,string profile,string template,string boot,string directory){
            Launch(acad,profile,template,boot,directory,null);
        }
        public void Launch(string acad,string profile,string template,string boot,string directory,string earlyModule){
            CommandLine=BuildCadCommandLine(acad,profile,template,boot,earlyModule);
            LaunchRaw(acad,directory);
        }
        // Only used by the separate no-CAD cross-desktop machine test.
        public void LaunchTestHost(string executable,string title,string directory){
            if(Path.GetFileName(executable)!="IsolatedDesktopGuiTestHost.exe")throw new ArgumentException("dedicated test helper required");
            CommandLine=Native.Quote(executable)+" "+Native.Quote(title);LaunchRaw(executable,directory);
        }
        void LaunchRaw(string acad,string directory){
            if(Created)throw new InvalidOperationException("one worker only");
            Native.StartupInfo si=new Native.StartupInfo();si.Cb=(uint)Marshal.SizeOf(typeof(Native.StartupInfo));si.Desktop=DesktopPath;si.Flags=0x00000001|0x00000080;si.ShowWindow=4;
            Native.ProcessInfo info;if(!Native.CreateProcessW(acad,new StringBuilder(CommandLine),IntPtr.Zero,IntPtr.Zero,false,0x00000200,IntPtr.Zero,directory,ref si,out info))throw Native.Error("CreateProcess owned AutoCAD");
            process=info.Process;initialThread=info.Thread;ProcessId=info.ProcessId;InitialThreadId=info.ThreadId;Created=true;
            RefreshIdentity();
            if(!String.Equals(Path.GetFullPath(acad),Image,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("created executable differs; owned identity retained, no automatic kill");
        }
        public void RefreshIdentity(){if(!Created)throw new InvalidOperationException("worker not created");Image=Native.ProcessImage(process);CreationFileTime=Native.ProcessCreation(process);CreationUtc=DateTime.FromFileTimeUtc(Int64.Parse(CreationFileTime,CultureInfo.InvariantCulture)).ToString("o");}
        public bool HasExited {get{if(!Created)return false;uint result=Native.WaitForSingleObject(process,0);if(result==Native.WAIT_FAILED)throw Native.Error("owned process wait");return result==Native.WAIT_OBJECT_0;}}
        public uint ExitCode {get{uint code;if(!Native.GetExitCodeProcess(process,out code))throw Native.Error("owned exit code");return code;}}
        public WindowRecord[] Windows(){
            return Native.DesktopWindows(desktop,DesktopName,ProcessId);
        }
        public StartupTipAction PostCloseBenignStartupTip(long hwnd){
            StartupTipAction action=new StartupTipAction();action.Utc=DateTime.UtcNow.ToString("o");action.DesktopName=DesktopName;action.ProcessId=ProcessId;action.Hwnd=hwnd;action.Status="refused_not_exact_owned_tip";
            Native.RequirePrivateName(DesktopName);
            if(!Created||HasExited){action.Status="refused_worker_not_live";return action;}
            foreach(WindowRecord record in Windows()){
                if(record.Hwnd!=hwnd)continue;
                action.Thread=record.ThreadAtEnumeration;action.ClassName=record.ClassName;action.Title=record.Title;
                if(record.PidAtEnumeration!=ProcessId||record.ClassName!="#32770"||record.Title!="日积月累")return action;
                IntPtr target=new IntPtr(hwnd);uint pid;uint thread=Native.GetWindowThreadProcessId(target,out pid);
                if(pid!=ProcessId||thread!=record.ThreadAtEnumeration||Native.ClassName(target)!="#32770")return action;
                StringBuilder title=new StringBuilder(4096);Native.GetWindowTextW(target,title,title.Capacity);if(title.ToString()!="日积月累")return action;
                action.Matched=true;action.Posted=Native.PostMessageW(target,0x0010,UIntPtr.Zero,IntPtr.Zero);action.Error=action.Posted?0:Marshal.GetLastWin32Error();action.Status=action.Posted?"posted_WM_CLOSE_exact_owned_startup_tip":"post_failed";return action;
            }
            return action;
        }
        public string InitialThreadDesktop(){return Native.ThreadDesktopName(InitialThreadId);}
        public void Dispose(){
            bool a=true,b=true;if(initialThread!=IntPtr.Zero){a=Native.CloseHandle(initialThread);initialThread=IntPtr.Zero;}if(process!=IntPtr.Zero){b=Native.CloseHandle(process);process=IntPtr.Zero;}ProcessHandlesClosed=a&&b;
            if(desktop!=IntPtr.Zero){DesktopClosed=Native.CloseDesktop(desktop);desktop=IntPtr.Zero;}
        }
    }
}
