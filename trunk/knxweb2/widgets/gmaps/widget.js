function CGMAPS(conf) {
	this.isResizable=true;
  this.init(conf);
  this.refreshHTML();
}

CGMAPS.type='gmaps';
UIController.registerWidget(CGMAPS);
CGMAPS.prototype = new CWidget();

// Refresh HTML from config
CGMAPS.prototype.refreshHTML = function() {
    $("div:first-child", this.div).html("<iframe id='my_maps_frame' src='widgets/gmaps/gmaps3.html' width='100%' height='100%' />");
}

// Called by eibcommunicator when a feedback object value has changed
CGMAPS.prototype.updateObject = function(obj,value) {

	if (obj==this.conf.getAttribute("feedback-object"))
	{
            if (!value)
                return;

	    var iframe = document.getElementById("my_maps_frame");
            if (!iframe.contentWindow.addMarker)
                return;

            if (this.currLocation == value)
                return;

            this.currLocation = value;
            var n = value.split(";", 2); 
            if (!this.marker)
                this.marker = iframe.contentWindow.addMarker(n[0], n[1], "", true);
            else
                iframe.contentWindow.updateMarker(this.marker, n[0], n[1], "", true);
	}
};
