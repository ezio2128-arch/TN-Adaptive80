using System;
using System.Globalization;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.ApplicationModel.AppService;
using Windows.Data.Json;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Microsoft.Gaming.XboxGameBar;

namespace FramePulse.Widget {
 public sealed partial class MainPage : Page {
    readonly DispatcherTimer timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
    bool busy, configured, editing, stopped;
    DateTime lastLaunch = DateTime.MinValue;
    string mode = "Adaptive";
    JsonArray points;
    XboxGameBarWidget widget;
    public MainPage() {
        InitializeComponent(); timer.Tick += Tick;
        Loaded += async (s,e) => {
            widget = App.Widget;
            if (widget != null) {
                widget.RequestedOpacityChanged += OpacityChanged;
                widget.ClickThroughEnabledChanged += ClickThroughChanged;
                widget.VisibleChanged += VisibleChanged;
                UpdateChrome();
            }
            await StartCore(); timer.Start();
            try { var startup=await StartupTask.GetAsync("FramePulseStartup");Startup.IsChecked=startup.State==StartupTaskState.Enabled; }
            catch(Exception ex) { Error.Text=ex.Message; }
        };
        Unloaded += (s,e) => {
            timer.Stop();
            if (widget != null) {
                widget.RequestedOpacityChanged -= OpacityChanged;
                widget.ClickThroughEnabledChanged -= ClickThroughChanged;
                widget.VisibleChanged -= VisibleChanged;
            }
        };
    }
    async Task StartCore() {
        if (stopped || DateTime.UtcNow-lastLaunch < TimeSpan.FromSeconds(5)) return;
        lastLaunch = DateTime.UtcNow;
        try { await FullTrustProcessLauncher.LaunchFullTrustProcessForCurrentAppAsync(); }
        catch (Exception e) { Error.Text = e.Message; }
    }
    async void OpacityChanged(XboxGameBarWidget w, object a) { await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, UpdateChrome); }
    async void ClickThroughChanged(XboxGameBarWidget w, object a) { await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, UpdateChrome); }
    async void VisibleChanged(XboxGameBarWidget w, object a) { await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => { if(w.Visible)timer.Start();else timer.Stop(); }); }
    void UpdateChrome() {
        if(widget == null)return;
        Surface.Opacity = Math.Max(.05,widget.RequestedOpacity/100.0);
        Surface.IsHitTestVisible = !widget.ClickThroughEnabled;
    }
    async Task<JsonObject> Send(ValueSet message) {
        var connection=App.Connection;
        if(connection == null) { await StartCore();return null; }
        try {
            var reply = await connection.SendMessageAsync(message);
            if(reply.Status != AppServiceResponseStatus.Success) { Error.Text="Core connection unavailable";return null; }
            if(reply.Message.ContainsKey("error")) { Error.Text=(string)reply.Message["error"];return null; }
            return reply.Message.ContainsKey("json") ? JsonObject.Parse((string)reply.Message["json"]) : null;
        } catch(Exception e) { Error.Text=e.Message;return null; }
    }
    async void Tick(object sender,object args) {
        if(busy || editing || stopped)return;busy=true;
        try {
            var data=await Send(new ValueSet { ["op"]="snapshot" });
            if(data == null)return;
            var fps=data.GetNamedNumber("fps",0);
            Fps.Text=fps>0?fps.ToString("F0",CultureInfo.InvariantCulture):"—";
            Frametime.Text=fps>0?data.GetNamedNumber("frametime",0).ToString("F2",CultureInfo.InvariantCulture)+" ms":"— ms";
            Game.Text=data.GetNamedString("game","");
            Metrics.Text=$"1% Low {data.GetNamedNumber("low1",0):F0}   Target {data.GetNamedNumber("target",0):F0}   Display {data.GetNamedNumber("refresh",0):F2} Hz";
            State.Text=data.GetNamedString("state", "IDLE");Pacing.Text=data.GetNamedString("pacing", "");
            Details.Text=$"PID {data.GetNamedNumber("pid",0):F0} · VRR: {data.GetNamedString("vrr", "UNKNOWN")}\n{data.GetNamedString("fg", "UNKNOWN")}";
            Error.Text=data.GetNamedString("error", "")+" "+data.GetNamedString("warning", "");
            Advanced.IsChecked=!data.GetNamedBoolean("safe",true);
            FgOff.IsChecked=data.GetNamedBoolean("fgOffConfirmed",false);
            string game=Game.Text;
            if(!configured || Target.Tag as string != game) {
                var cfg=data.GetNamedObject("config");mode=cfg.GetNamedString("mode");ModeLabel.Text=mode;
                Target.Text=cfg.GetNamedNumber(mode=="Manual"?"manual":"maximum").ToString(CultureInfo.InvariantCulture);
                Minimum.Text=cfg.GetNamedNumber("minimum").ToString(CultureInfo.InvariantCulture);
                string r=cfg.GetNamedString("response");Response.SelectedIndex=r=="Smooth"?0:r=="Responsive"?2:1;
                Target.Tag=game;configured=true;
            }
            points=data.GetNamedArray("graph");Draw();
        } catch(Exception e) { Error.Text=e.Message; }
        finally { busy=false; }
    }
    void Draw() {
        if(points==null || GraphEnabled.IsChecked!=true)return;
        var p=new PointCollection();double max=20;
        foreach(var v in points)max=Math.Max(max,Math.Min(100,v.GetNumber()));
        for(int i=0;i<points.Count;i++)p.Add(new Point(i*Math.Max(1,Graph.ActualWidth-2)/Math.Max(1,points.Count-1),
            62-Math.Min(100,points[i].GetNumber())/max*60));
        Trace.Points=p;
    }
    void GraphSizeChanged(object sender,SizeChangedEventArgs e) { Draw(); }
    void GraphToggle(object sender,RoutedEventArgs e) { Graph.Visibility=GraphEnabled.IsChecked==true?Visibility.Visible:Visibility.Collapsed; }
    async Task Apply() {
        editing=true;
        try {
            double target,min;
            if(!double.TryParse(Target.Text,NumberStyles.Float,CultureInfo.InvariantCulture,out target)||
               !double.TryParse(Minimum.Text,NumberStyles.Float,CultureInfo.InvariantCulture,out min)||
               double.IsNaN(target)||double.IsInfinity(target)||target<10||target>1000||min<10||min>target) {
                Error.Text="Enter finite FPS in 10..1000; minimum <= maximum";return;
            }
            var cfg=new JsonObject {
                ["mode"]=JsonValue.CreateStringValue(mode),
                ["minimum"]=JsonValue.CreateNumberValue(min),
                ["maximum"]=JsonValue.CreateNumberValue(target),
                ["manual"]=JsonValue.CreateNumberValue(target),
                ["response"]=JsonValue.CreateStringValue((Response.SelectedItem as ComboBoxItem)?.Content as string ?? "Balanced")
            };
            if(await Send(new ValueSet { ["op"]="configure", ["config"]=cfg.Stringify() })!=null) { ModeLabel.Text=mode;Error.Text="Profile saved"; }
        } finally { editing=false; }
    }
    async void ApplyClick(object sender,RoutedEventArgs e) { await Apply(); }
    async void AdaptiveClick(object sender,RoutedEventArgs e) { mode="Adaptive";await Apply(); }
    async void ManualClick(object sender,RoutedEventArgs e) { mode="Manual";await Apply(); }
    async void AdvancedClick(object sender,RoutedEventArgs e) {
        await Send(new ValueSet { ["op"]="safe", ["value"]=Advanced.IsChecked!=true });
    }
    async void FgOffClick(object sender,RoutedEventArgs e) {
        await Send(new ValueSet { ["op"]="fgOff", ["value"]=FgOff.IsChecked==true });
    }
    async void AttachClick(object sender,RoutedEventArgs e) {
        await Send(new ValueSet { ["op"]="attach" });
    }
    async void LogsClick(object sender,RoutedEventArgs e) {
        await Send(new ValueSet { ["op"]="openLogs" });
    }
    async void StartupClick(object sender,RoutedEventArgs e) {
        try {
            var task=await StartupTask.GetAsync("FramePulseStartup");
            if(Startup.IsChecked==true) {
                var result=await task.RequestEnableAsync();
                Startup.IsChecked=result==StartupTaskState.Enabled;
                if(Startup.IsChecked!=true)Error.Text="Windows did not enable startup; check Task Manager > Startup apps";
            } else task.Disable();
        } catch(Exception ex) { Error.Text=ex.Message;Startup.IsChecked=false; }
    }
    async void DebugClick(object sender,RoutedEventArgs e) {
        await Send(new ValueSet { ["op"]="debug", ["value"]=(sender as CheckBox)?.IsChecked==true });
    }
    async void StopClick(object sender,RoutedEventArgs e) { stopped=true;timer.Stop();await Send(new ValueSet { ["op"]="shutdown" }); }
 }
}
