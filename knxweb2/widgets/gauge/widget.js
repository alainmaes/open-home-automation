function CGAUGE(conf) {
  this.isResizable=true;
  this.initialized = false;
  this.init(conf);
  this.refreshHTML();
}

CGAUGE.type='gauge';
UIController.registerWidget(CGAUGE);
CGAUGE.prototype = new CWidget();

// Refresh HTML from config
CGAUGE.prototype.refreshHTML = function() {
    var link = "widgets/gauge/gauge.html";
    this.iframe = document.createElement('iframe');
    this.iframe.frameBorder=0;
    this.iframe.width="100%";
    this.iframe.height="100%";
    this.iframe.id="my_gauge_frame";
    this.iframe.setAttribute("src", link);
    $(this.div).append(this.iframe);

    //$("div:first-child", this.div).html("<iframe id='my_gauge_frame' src='widgets/gauge/gauge.html' width='100%' height='100%' frameBorder=0 />");
}

// Called by eibcommunicator when a feedback object value has changed
CGAUGE.prototype.updateObject = function(obj,value) {

	if (obj==this.conf.getAttribute("feedback-object"))
	{
            if (!value)
                return;

            if (!this.initialized)
            {
                var iframe = this.iframe; //.getElementById("my_gauge_frame");
                if (iframe)
                {
                    if (iframe.contentWindow.setLabel)
                    {
                        iframe.contentWindow.setLabel(this.conf.getAttribute("label"));
                        this.initialized = true;
                    }
                }
            }

            if (this.currValue == value)
                return;
            
            var iframe = this.iframe; //.getElementById("my_gauge_frame");
            
            if (iframe.contentWindow.setValue)
            {
               this.currValue = value;
               iframe.contentWindow.setValue(value);
            }
	}
};
