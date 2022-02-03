using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using ScoreBorad2.ViewModels;
using ScoreBorad2.Views;

namespace ScoreBorad2
{
    /// <summary>
    /// App.xaml の相互作用ロジック
    /// </summary>
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            var w = new Views.MainView();
            var vm = new ViewModels.ScoreBoradViewModel();
            w.DataContext = vm;
            w.Show();
        }
    }
}
