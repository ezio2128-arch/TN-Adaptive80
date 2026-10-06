using Microsoft.Gaming.XboxGameBar;
using Windows.ApplicationModel.Activation;
using Windows.ApplicationModel.AppService;
using Windows.ApplicationModel.Background;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

namespace FramePulse.Widget {
    sealed partial class App : Application {
        internal static AppServiceConnection Connection;
        internal static XboxGameBarWidget Widget;
        BackgroundTaskDeferral bridgeDeferral;
        public App() { InitializeComponent(); }
        protected override void OnActivated(IActivatedEventArgs args) {
            if (args.Kind != ActivationKind.Protocol) return;
            var protocol = args as ProtocolActivatedEventArgs;
            if (protocol == null || protocol.Uri.Scheme != "ms-gamebarwidget") return;
            var activation = args as XboxGameBarWidgetActivatedEventArgs;
            if (activation == null || !activation.IsLaunchActivation) return;
            var frame = new Frame(); Window.Current.Content = frame;
            Widget = new XboxGameBarWidget(activation, Window.Current.CoreWindow, frame);
            frame.Navigate(typeof(MainPage)); Window.Current.Activate();
            Window.Current.Closed += (s,e) => { Widget = null; };
        }
        protected override void OnLaunched(LaunchActivatedEventArgs args) {
            var frame = new Frame(); Window.Current.Content = frame;
            frame.Navigate(typeof(MainPage)); Window.Current.Activate();
        }
        protected override void OnBackgroundActivated(BackgroundActivatedEventArgs args) {
            var details = args.TaskInstance.TriggerDetails as AppServiceTriggerDetails;
            if (details == null || details.Name != "FramePulse.Bridge") return;
            // Reject connections from other package identities.
            if (details.CallerPackageFamilyName != Windows.ApplicationModel.Package.Current.Id.FamilyName) return;
            var thisDeferral = args.TaskInstance.GetDeferral();bridgeDeferral = thisDeferral;
            var thisConnection = details.AppServiceConnection; Connection = thisConnection;
            int completed = 0;
            System.Action finish = () => {
                if(System.Threading.Interlocked.Exchange(ref completed,1)==0)thisDeferral.Complete();
            };
            args.TaskInstance.Canceled += (s,e) => {
                if (Connection == thisConnection) { Connection = null;bridgeDeferral = null; }
                finish();
            };
            thisConnection.ServiceClosed += (s,e) => {
                if (Connection == thisConnection) Connection = null;
                finish();
            };
        }
    }
}
