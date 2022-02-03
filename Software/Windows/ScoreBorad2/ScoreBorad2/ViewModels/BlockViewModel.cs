using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Policy;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Documents;

namespace ScoreBorad2.ViewModels
{
    class BlockViewModel : NotificationObject
    {
        public BlockViewModel(string _blockname)
        {
            this.BlockName = _blockname;
        }

        private string _blockName;
        public string BlockName
        {
            get { return this._blockName; }
            set { SetProperty(ref this._blockName, value); }
        }

        private int _score;
        public int Score
        {
            get { return this._score; }
            set { 
                if(SetProperty(ref this._score, value))
                {
                    this.Plus100.RaiseCanExcuteChanged();
                    this.Plus10.RaiseCanExcuteChanged();
                    this.Plus1.RaiseCanExcuteChanged();
                    this.Minus1.RaiseCanExcuteChanged();
                    this.Minus10.RaiseCanExcuteChanged();
                    this.Minus100.RaiseCanExcuteChanged();
                } 
            }
        }

        private bool _disable;
        public bool Disable
        {
            get { return this._disable; }
            set { SetProperty(ref this._disable, value); }
        }


        //ボタンコマンド割り当て
        #region
        private DelegateCommand _plus100;
        public DelegateCommand Plus100
        {
            get
            {
                return this._plus100 ?? (this._plus100 = new DelegateCommand(_ =>
                {
                    Score+=100;
                },_ => Score < 900));
            }
        }

        private DelegateCommand _plus10;
        public DelegateCommand Plus10
        {
            get
            {
                return this._plus10 ?? (this._plus10 = new DelegateCommand(_ =>
                {
                    Score += 10;
                },_ => Score < 990));
            }
        }

        private DelegateCommand _plus1;
        public DelegateCommand Plus1
        {
            get
            {
                return this._plus1 ?? (this._plus1 = new DelegateCommand(_ =>
                {
                    Score += 1;
                },_ => Score < 999));
            }
        }

        private DelegateCommand _minus100;
        public DelegateCommand Minus100
        {
            get
            {
                return this._minus100 ?? (this._minus100 = new DelegateCommand(_ =>
                {
                    Score -= 100;
                }, _ => Score >= 100));
            }
        }

        private DelegateCommand _minus10;
        public DelegateCommand Minus10
        {
            get
            {
                return this._minus10 ?? (this._minus10 = new DelegateCommand(_ =>
                {
                    Score -= 10;
                }, _ => Score >= 10));
            }
        }

        private DelegateCommand _minus1;
        public DelegateCommand Minus1
        {
            get
            {
                return this._minus1 ?? (this._minus1 = new DelegateCommand(_ =>
                {
                    Score -= 1;
                }, _ => Score >= 1));
            }
        }
        #endregion a
    }
}
