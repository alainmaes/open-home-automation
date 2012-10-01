<html xmlns:v="urn:schemas-microsoft-com:vml">
<head>
		<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
		
		<meta http-equiv="pragma" content="no-cache" />
		<meta http-equiv="cache-control" content="no-cache, must-revalidate" />
		
		<title>{$_config.title}</title>
		{foreach from=$cssList item=css}
		<link rel="stylesheet" type="text/css" href="{$css}" />
		{/foreach}
		<script type="text/javascript" >
		var tab_config = {$_config|json_encode};
		var _widgets = {$widgets|json_encode};
		</script>
		{foreach from=$jsList item=js}
		<script type="text/javascript" src="{$js}"></script>
		{/foreach}
</head>
<body>

{if $_config.useJavaIfAvailable=='true'}
<applet code="org.knxweb.objectupdater.objectUpdater.class" archive="objectUpdater.jar" width="1" height="1">
    <param name="linknxHost" value="{$_config.linknx_host}">
    <param name="linknxPort" value="{$_config.linknx_port}">
</applet>
{/if}

<div id="widgetsTemplate" style="display: none;">

	{foreach from=$widgets key=id item=i}
		{include file="widgets/$id/widget.html"}
	{/foreach}

</div>


<div class="menuTitle"></div>
<div id="notificationZone" class="notification"></div>


<div id="screen">
<img class="prev" src="images/previous.png" alt="prev" width="42" height="53" />

	<div id="zoneContainer">
	</div>

<img class="next" src="images/next.png" alt="next" width="42" height="53" />
</div>

</body>
</html>