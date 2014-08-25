
package {

    import flash.display.Sprite;
	import flash.net.NetConnection;
	import flash.events.NetStatusEvent;
	import flash.events.SecurityErrorEvent;
	import flash.events.AsyncErrorEvent;
	import flash.media.Video;
	import flash.net.NetStream;
	import flash.net.ObjectEncoding;
	import flash.text.TextField;

    public class Player extends Sprite
	{
		private var _connection : NetConnection;
		private var _stream : NetStream;
		private var _video : Video;
		private var _textY : Number;

        public function Player()
		{
			_connection = new NetConnection();
			_connection.addEventListener(NetStatusEvent.NET_STATUS, onNetStatus);
			_connection.addEventListener(SecurityErrorEvent.SECURITY_ERROR, onSecurityError);
			_connection.objectEncoding = ObjectEncoding.AMF0;
			_connection.connect("rtmp://127.0.0.1:2935/fastplay/");
			_textY = 0;
        }

		private function onNetStatus( event : NetStatusEvent ) : void
		{
			var output:TextField = new TextField();
			output.width = 10000;
			output.text = event.info.code;
			output.antiAliasType = flash.text.AntiAliasType.ADVANCED
			output.x = 0;
			output.y = _textY;
			output.scaleX = output.scaleY = 0.5;
			addChild(output);
			_textY += 10;

			switch(event.info.code){
			case "NetConnection.Connect.Success":
				_stream = new NetStream(_connection);
				_stream.addEventListener(NetStatusEvent.NET_STATUS, onNetStatus);
				_stream.addEventListener(AsyncErrorEvent.ASYNC_ERROR, onAsyncError);

				var video : Video = new Video(320, 180);
				video.attachNetStream(_stream);
				video.deblocking = 2;
				video.smoothing = true;

				_stream.play("test.mp4");
				addChild(video);
				_video = video
				break;

			case "NetStream.Play.StreamNotFound":
				trace("File not found");
				break;
			default:
				break;
			}
		}

		private function onSecurityError( event : SecurityErrorEvent ) : void
		{
		}

		private function onAsyncError( event : AsyncErrorEvent ) : void
		{
		}
    }
}
