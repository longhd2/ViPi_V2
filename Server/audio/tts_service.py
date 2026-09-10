class TTSService:
    """Provider-neutral TTS service. Produces audio; never owns client I2S."""
    def __init__(self,provider=None): self.provider=provider
    async def synthesize(self,text,voice=None):
        if not self.provider: return {'text':text,'audio':None,'format':'opus','sample_rate':16000}
        return await self.provider.synthesize(text,voice=voice)
