import ratapi

app = ratapi.create_app(
    config_override={
        'APPLICATION_ROOT': '/fbrat',
    }
)
