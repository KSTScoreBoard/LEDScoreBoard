using ScoreBorad2.Properties;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO.Ports;
namespace ScoreBorad2.ViewModels
{
    class ScoreBoradViewModel : NotificationObject
    {
        static SerialPort SerialPort = new SerialPort("COM10", 115200, Parity.None, 8, StopBits.One);
        Serial Serial;
  
        public ScoreBoradViewModel()
        {
            if(SerialPort.IsOpen) SerialPort.Close();
            _block1 = new BlockViewModel(Settings.Default.BlockName1);
            _block2 = new BlockViewModel(Settings.Default.BlockName2);
            _block3 = new BlockViewModel(Settings.Default.BlockName3);
            _block4 = new BlockViewModel(Settings.Default.BlockName4);
            SerialPort.Open();
            Serial = new Serial(SerialPort);
        }

        private BlockViewModel _block1;
        public BlockViewModel Block1
        {
            get { return this._block1; }
            set { SetProperty(ref this._block1, value); }
        }

        private BlockViewModel _block2;
        public BlockViewModel Block2
        {
            get { return this._block2; }
            set { SetProperty(ref this._block2, value); }
        }

        private BlockViewModel _block3;
        public BlockViewModel Block3
        {
            get { return this._block3; }
            set { SetProperty(ref this._block3, value); }
        }
        private BlockViewModel _block4;
        public BlockViewModel Block4
        {
            get { return this._block4; }
            set { SetProperty(ref this._block4, value); }
        }

        private bool _toggle1;

        public bool Toggle1
        {
            get { return this._toggle1; }
            set { SetProperty(ref this._toggle1, value); }
        }

        private bool _toggle10;

        public bool Toggle10
        {
            get { return this._toggle10; }
            set { SetProperty(ref this._toggle10, value); }
        }

        private bool _toggle100;

        public bool Toggle100
        {
            get { return this._toggle100; }
            set { SetProperty(ref this._toggle100, value); }
        }

        private DelegateCommand _send;

        public DelegateCommand Send
        {
            get
            {
                return this._send ?? (this._send = new DelegateCommand(_ =>
                {
                    Serial.SetScore(Block1.Score, Block2.Score, Block3.Score, Block4.Score);
                    Serial.SetMode( new Mode(Toggle1, Toggle10, Toggle100, Block1.Disable),
                                    new Mode(Toggle1, Toggle10, Toggle100, Block2.Disable),
                                    new Mode(Toggle1, Toggle10, Toggle100, Block3.Disable),
                                    new Mode(Toggle1, Toggle10, Toggle100, Block4.Disable));
                    Serial.Send();

                }));
            }
        }


    }
}
