using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ScoreBorad2
{
    using System;
    using System.Windows.Input;
    class DelegateCommand : ICommand
    {
        private Action<object> _excute;

        private Func<object, bool> _canExcute;

        public DelegateCommand(Action<object> excute) : this(excute, null)
        {

        }

        public DelegateCommand(Action<object> excute, Func<object, bool> canExcute)
        {
            this._excute = excute;
            this._canExcute = canExcute;
        }

        public event EventHandler CanExecuteChanged;

        public bool CanExecute(object parameter)
        {
            return (this._canExcute != null) ? this._canExcute(parameter) : true;
        }

        public void Execute(object parameter)
        {
            if (this._excute != null)
            {
                this._excute(parameter);

            }
        }

        public void RaiseCanExcuteChanged()
        {
            var h = this.CanExecuteChanged;
            if (h != null) h(this, EventArgs.Empty);
        }
    }
}
